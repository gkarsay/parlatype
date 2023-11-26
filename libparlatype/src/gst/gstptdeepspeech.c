/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* DeepSpeech plugin for Parlatype with some boilerplate from Mike Sheldon's
 * gst-deepspeech but different implementation. vad_private.c and vad_private.h
 * are taken unmodified from gst-remove-silence (GStreamer bad plugins). */

#include <config.h>

#include <gst/gst.h>
#include <deepspeech.h>
#include <string.h>

#include "vad_private.h"
#include "gstptdeepspeech.h"

GST_DEBUG_CATEGORY_STATIC (gst_ptdeepspeech_debug);
#define GST_CAT_DEFAULT gst_ptdeepspeech_debug

#define DEFAULT_BEAM_WIDTH 500
#define DEFAULT_VAD_HYSTERESIS 480 /* 60 mseg */
#define DEFAULT_VAD_THRESHOLD -60

#define INTERMEDIATE FALSE
#define FINAL TRUE

enum
{
  PROP_0,
  PROP_SPEECH_MODEL,
  PROP_SCORER,
  PROP_BEAM_WIDTH,
  PROP_VAD_MIN_SILENCE,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = {
  NULL,
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
                                                                    GST_PAD_SINK,
                                                                    GST_PAD_ALWAYS,
                                                                    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=16000,channels=1"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
                                                                   GST_PAD_SRC,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=16000,channels=1"));

#define gst_ptdeepspeech_parent_class parent_class
G_DEFINE_TYPE (GstPtDeepspeech, gst_ptdeepspeech, GST_TYPE_ELEMENT);

static gboolean
gst_ptdeepspeech_load_model (GstPtDeepspeech *self)
{
  int status;

  status = DS_CreateModel (self->speech_model_path, &self->model_state);
  if (status != DS_ERR_OK)
    {
      GST_ELEMENT_ERROR (GST_ELEMENT (self), LIBRARY, SETTINGS,
                         ("%s", DS_ErrorCodeToErrorMessage (status)),
                         ("model path: %s", self->speech_model_path));
      return FALSE;
    }

  status = DS_SetModelBeamWidth (self->model_state, self->beam_width);
  if (status != DS_ERR_OK)
    {
      GST_ELEMENT_ERROR (GST_ELEMENT (self), LIBRARY, SETTINGS,
                         ("%s", DS_ErrorCodeToErrorMessage (status)),
                         ("beam width: %d", self->beam_width));
      return FALSE;
    }

  status = DS_EnableExternalScorer (self->model_state, self->scorer_path);
  if (status != DS_ERR_OK)
    {
      GST_ELEMENT_ERROR (GST_ELEMENT (self), LIBRARY, SETTINGS,
                         ("%s", DS_ErrorCodeToErrorMessage (status)),
                         ("scorer path: %s", self->scorer_path));
      return FALSE;
    }

  status = DS_CreateStream (self->model_state, &self->streaming_state);
  if (status != DS_ERR_OK)
    {
      GST_ELEMENT_ERROR (GST_ELEMENT (self), LIBRARY, SETTINGS,
                         ("%s", DS_ErrorCodeToErrorMessage (status)),
                         (NULL));
      return FALSE;
    }

  return TRUE;
}

static GstMessage *
gst_ptdeepspeech_message_new (GstPtDeepspeech *self,
                              GstBuffer *buf,
                              const char *text,
                              gboolean final)
{
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstStructure *s;

  if (buf)
    timestamp = GST_BUFFER_TIMESTAMP (buf);

  s = gst_structure_new ("deepspeech",
                         "timestamp", G_TYPE_UINT64, timestamp,
                         "final", G_TYPE_BOOLEAN, final,
                         "hypothesis", G_TYPE_STRING, text,
                         NULL);

  return gst_message_new_element (GST_OBJECT (self), s);
}

static void
get_text (GstPtDeepspeech *self,
          GstBuffer *buf,
          gboolean final)
{
  GstMessage *msg;
  char *result;
  int status;

  if (final)
    result = DS_FinishStream (self->streaming_state);
  else
    result = DS_IntermediateDecode (self->streaming_state);

  if (strlen (result) > 0)
    {
      msg = gst_ptdeepspeech_message_new (self, buf, result, final);
      gst_element_post_message (GST_ELEMENT (self), msg);
    }

  DS_FreeString (result);

  if (final)
    {
      /* Recreate stream, we still need it */
      status = DS_CreateStream (self->model_state, &self->streaming_state);
      if (status != DS_ERR_OK)
        {
          GST_ELEMENT_ERROR (GST_ELEMENT (self), LIBRARY, SETTINGS,
                             ("%s", DS_ErrorCodeToErrorMessage (status)),
                             (NULL));
        }
    }
}

static GstStateChangeReturn
gst_ptdeepspeech_change_state (GstElement *element,
                               GstStateChange transition)
{
  GstPtDeepspeech *self = GST_PTDEEPSPEECH (element);
  GstStateChangeReturn ret;
  GstEvent *seek = NULL;
  GstSegment *seg = NULL;
  gboolean success;

  /* handle upward state changes */
  switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_ptdeepspeech_load_model (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
    }

  ret = GST_ELEMENT_CLASS (gst_ptdeepspeech_parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  /* handle downward state changes */
  switch (transition)
    {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      if (self->eos)
        break;
      /* Getting the final result – crashes sometimes */
      // get_text (self, NULL, FINAL);
      /* Work around a problem with unknown cause:
       * When changing to paused position queries return the running
       * time instead of stream time */
      seg = &self->segment;
      if (seg->position > seg->stop)
        seg->position = seg->stop;
      seek = gst_event_new_seek (1.0, GST_FORMAT_TIME,
                                 GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
                                 GST_SEEK_TYPE_SET, seg->position,
                                 GST_SEEK_TYPE_NONE, -1);
      success = gst_pad_push_event (self->sinkpad, seek);
      GST_DEBUG_OBJECT (self, "pushed segment event: %s", success ? "yes" : "no");
      /* This seems to have the same effect: */
      /*success = gst_pad_push_event (self->srcpad, gst_event_new_flush_start ());
      GST_DEBUG_OBJECT (self, "flush-start event %s", success ? "sent" : "not sent");
      success = gst_pad_push_event (self->srcpad, gst_event_new_flush_stop (TRUE));
      GST_DEBUG_OBJECT (self, "flush-stop event %s", success ? "sent" : "not sent");*/

      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      DS_FreeModel (self->model_state);
      break;
    default:
      break;
    }

  return ret;
}

static GstFlowReturn
gst_ptdeepspeech_chain (GstPad *pad,
                        GstObject *parent,
                        GstBuffer *buf)
{
  GstPtDeepspeech *self = GST_PTDEEPSPEECH (parent);

  GstMapInfo info;
  int frame_type;
  gboolean final = FALSE;
  GstClockTime position, duration;

  /* Don't bother with other states then playing */
  if (GST_STATE (GST_ELEMENT (self)) != GST_STATE_PLAYING)
    return gst_pad_push (self->srcpad, buf);

  /* Track current position */
  position = GST_BUFFER_TIMESTAMP (buf);
  if (GST_CLOCK_TIME_IS_VALID (position))
    {
      duration = GST_BUFFER_DURATION (buf);
      if (GST_CLOCK_TIME_IS_VALID (duration))
        {
          position += duration;
        }
      self->segment.position = position;
    }

  gst_buffer_map (buf, &info, GST_MAP_READ);

  /* Is the current buffer silent? */
  frame_type = vad_update (self->vad, (gint16 *) info.data, info.size / sizeof (gint16));
  if (frame_type == VAD_SILENCE && self->in_speech)
    {
      if (GST_BUFFER_DURATION_IS_VALID (buf))
        {
          self->silence_time += duration;
        }
      else
        {
          GST_WARNING ("Invalid buffer duration, consecutive_silence_time update not possible");
        }

      if (self->silence_time >= (self->vad_min_silence * GST_MSECOND))
        {
          self->in_speech = FALSE;
          final = TRUE;
        }
    }

  if (frame_type != VAD_SILENCE)
    {
      self->silence_time = 0;
      self->in_speech = TRUE;
    }

  /* Feed DeepSpeech with data */
  DS_FeedAudioContent (self->streaming_state,
                       (const short *) info.data,
                       (unsigned int) info.size / sizeof (short));

  /* Get either a final result or an intermediate result every 500
   * milliseconds */
  if (final)
    {
      get_text (self, buf, FINAL);
      self->last_result_time = position;
    }
  else
    {
      if (self->in_speech &&
          (position - self->last_result_time) > GST_MSECOND * 500)
        {
          get_text (self, buf, INTERMEDIATE);
          self->last_result_time = position;
        }
    }

  return gst_pad_push (self->srcpad, buf);
}

static gboolean
gst_ptdeepspeech_event (GstPad *pad,
                        GstObject *parent,
                        GstEvent *event)
{
  GstPtDeepspeech *self = GST_PTDEEPSPEECH (parent);
  gboolean ret;

  GST_LOG_OBJECT (self, "Received %s event: %" GST_PTR_FORMAT,
                  GST_EVENT_TYPE_NAME (event), (GST_EVENT_TYPE (event) == GST_EVENT_TAG) ? NULL : event);
  self->eos = FALSE;

  switch (GST_EVENT_TYPE (event))
    {
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
      g_assert (gst_event_has_name (event, "unblock"));
      gst_event_unref (event);
      return TRUE;
    case GST_EVENT_CAPS:;
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_EOS:
      get_text (self, NULL, FINAL);
      ret = gst_pad_event_default (pad, parent, event);
      self->eos = TRUE;
      break;
    case GST_EVENT_SEGMENT:
      /* keep current segment, used for tracking current position */
      gst_event_copy_segment (event, &self->segment);
      /* fall through */
    default:
      ret = gst_pad_event_default (pad, parent, event);
    }
  return ret;
}

static void
gst_ptdeepspeech_finalize (GObject *object)
{
  GstPtDeepspeech *self = GST_PTDEEPSPEECH (object);

  vad_destroy (self->vad);
  self->vad = NULL;

  g_free (self->speech_model_path);
  g_free (self->scorer_path);

  G_OBJECT_CLASS (gst_ptdeepspeech_parent_class)->finalize (object);
}

static void
gst_ptdeepspeech_init (GstPtDeepspeech *self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (self->sinkpad,
                              GST_DEBUG_FUNCPTR (gst_ptdeepspeech_event));
  gst_pad_set_chain_function (self->sinkpad,
                              GST_DEBUG_FUNCPTR (gst_ptdeepspeech_chain));
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (self->srcpad);
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);

  self->vad = vad_new (DEFAULT_VAD_HYSTERESIS, DEFAULT_VAD_THRESHOLD);
  self->in_speech = FALSE;
  self->silence_time = 0;
  self->last_result_time = GST_CLOCK_TIME_NONE;
  self->eos = FALSE;
}

static void
gst_ptdeepspeech_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  GstPtDeepspeech *self = GST_PTDEEPSPEECH (object);

  switch (prop_id)
    {
    case PROP_SPEECH_MODEL:
      g_free (self->speech_model_path);
      self->speech_model_path = g_value_dup_string (value);
      break;
    case PROP_SCORER:
      g_free (self->scorer_path);
      self->scorer_path = g_value_dup_string (value);
      break;
    case PROP_BEAM_WIDTH:
      self->beam_width = g_value_get_int (value);
      break;
    case PROP_VAD_MIN_SILENCE:
      self->vad_min_silence = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_ptdeepspeech_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  GstPtDeepspeech *self = GST_PTDEEPSPEECH (object);

  switch (prop_id)
    {
    case PROP_SPEECH_MODEL:
      g_value_set_string (value, self->speech_model_path);
      break;
    case PROP_SCORER:
      g_value_set_string (value, self->scorer_path);
      break;
    case PROP_BEAM_WIDTH:
      g_value_set_int (value, self->beam_width);
      break;
    case PROP_VAD_MIN_SILENCE:
      g_value_set_int (value, self->vad_min_silence);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_ptdeepspeech_class_init (GstPtDeepspeechClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  ;

  object_class->set_property = gst_ptdeepspeech_set_property;
  object_class->get_property = gst_ptdeepspeech_get_property;
  object_class->finalize = gst_ptdeepspeech_finalize;

  element_class->change_state = gst_ptdeepspeech_change_state;

  obj_properties[PROP_SPEECH_MODEL] =
      g_param_spec_string (
          "speech-model",
          "Speech Model",
          "Location of the speech graph file.",
          NULL,
          G_PARAM_READWRITE);

  obj_properties[PROP_SCORER] =
      g_param_spec_string (
          "scorer",
          "Scorer",
          "Location of the scorer file.",
          NULL,
          G_PARAM_READWRITE);

  obj_properties[PROP_BEAM_WIDTH] =
      g_param_spec_int (
          "beam-width",
          "Beam Width",
          "The beam width used by the decoder. A larger beam width generates better results at the cost of decoding time.",
          0, G_MAXINT, DEFAULT_BEAM_WIDTH,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  obj_properties[PROP_VAD_MIN_SILENCE] =
      g_param_spec_int (
          "vad-min-silence",
          "VAD minimum silence",
          "The minimum time of silence in microseconds for voice activity detection",
          0, G_MAXINT, 500,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);

  gst_element_class_set_static_metadata (
      element_class,
      "Parlatype Deepspeech",
      "Filter/Audio",
      "Performs speech recognition using the Mozilla DeepSpeech model",
      "Gabor Karsay <gabor.karsay@gmx.at>");

  gst_element_class_add_pad_template (
      element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (
      element_class,
      gst_static_pad_template_get (&sink_factory));
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_ptdeepspeech_debug, "ptdeepspeech", 0,
                           "Performs speech recognition using Mozilla's DeepSpeech model.");

  if (!gst_element_register (plugin,
                             "ptdeepspeech",
                             GST_RANK_NONE,
                             GST_TYPE_PTDEEPSPEECH))
    return FALSE;
  return TRUE;
}

/**
 * gst_ptdeepspeech_register:
 *
 * Registers a plugin holding our single element to use privately in this
 * library.
 *
 * Return value: TRUE if successful, otherwise FALSE
 */
gboolean
gst_ptdeepspeech_register (void)
{
  return gst_plugin_register_static (
      GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "ptdeepspeech",
      "PtDeepspeech plugin",
      plugin_init,
      PACKAGE_VERSION,
      "LGPL",
      "libparlatype",
      "Parlatype",
      "https://www.parlatype.org/");
}
