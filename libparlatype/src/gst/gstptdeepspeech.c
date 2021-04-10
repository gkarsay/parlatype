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
#define DEFAULT_VAD_HYSTERESIS  480     /* 60 mseg */
#define DEFAULT_VAD_THRESHOLD -60

#define INTERMEDIATE FALSE
#define FINAL        TRUE

enum
{
	PROP_0,
	PROP_SPEECH_MODEL,
	PROP_SCORER,
	PROP_BEAM_WIDTH,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=16000,channels=1")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=16000,channels=1")
    );

#define gst_ptdeepspeech_parent_class parent_class
G_DEFINE_TYPE (GstPtDeepspeech, gst_ptdeepspeech, GST_TYPE_ELEMENT);


static gboolean
gst_ptdeepspeech_load_model (GstPtDeepspeech *deepspeech)
{
	int status;

	status = DS_CreateModel (deepspeech->speech_model_path, &deepspeech->model_state);
	if (status != DS_ERR_OK) {
		GST_ELEMENT_ERROR (GST_ELEMENT(deepspeech), LIBRARY, SETTINGS,
		                  ("%s", DS_ErrorCodeToErrorMessage(status)),
		                  ("model path: %s", deepspeech->speech_model_path));
		return FALSE;
	}

	status = DS_SetModelBeamWidth (deepspeech->model_state, deepspeech->beam_width);
	if (status != DS_ERR_OK) {
		GST_ELEMENT_ERROR (GST_ELEMENT(deepspeech), LIBRARY, SETTINGS,
		                  ("%s", DS_ErrorCodeToErrorMessage(status)),
		                  ("beam width: %d", deepspeech->beam_width));
		return FALSE;
	}

	status = DS_EnableExternalScorer (deepspeech->model_state, deepspeech->scorer_path);
	if (status != DS_ERR_OK) {
		GST_ELEMENT_ERROR (GST_ELEMENT(deepspeech), LIBRARY, SETTINGS,
		                  ("%s", DS_ErrorCodeToErrorMessage(status)),
		                  ("scorer path: %s", deepspeech->scorer_path));
		return FALSE;
	}

	status = DS_CreateStream (deepspeech->model_state, &deepspeech->streaming_state);
	if (status != DS_ERR_OK) {
		GST_ELEMENT_ERROR (GST_ELEMENT(deepspeech), LIBRARY, SETTINGS,
		                  ("%s", DS_ErrorCodeToErrorMessage(status)),
		                  (NULL));
		return FALSE;
	}

	return TRUE;
}

static GstMessage *
gst_ptdeepspeech_message_new (GstPtDeepspeech *deepspeech,
                              GstBuffer       *buf,
                              const char      *text,
                              gboolean         final)
{
	GstClockTime timestamp = GST_CLOCK_TIME_NONE;
	GstStructure *s;

	if (buf)
		timestamp = GST_BUFFER_TIMESTAMP (buf);

	s = gst_structure_new ("deepspeech",
	                       "timestamp",  G_TYPE_UINT64,  timestamp,
	                       "final",      G_TYPE_BOOLEAN, final,
	                       "hypothesis", G_TYPE_STRING,  text,
	                        NULL);

	return gst_message_new_element (GST_OBJECT (deepspeech), s);
}

static void
get_text (GstPtDeepspeech *deepspeech,
          GstBuffer       *buf,
          gboolean         final)
{
	GstMessage *msg;
	char       *result;
	int         status;

	if (final)
		result = DS_FinishStream (deepspeech->streaming_state);
	else
		result = DS_IntermediateDecode (deepspeech->streaming_state);

	g_print ("result: %s\n", result);
	if (strlen(result) > 0) {
		msg = gst_ptdeepspeech_message_new (deepspeech, buf, result, final);
		gst_element_post_message (GST_ELEMENT (deepspeech), msg);
	}

	DS_FreeString (result);

	if (final) {
		/* Recreate stream, we still need it */
		status = DS_CreateStream (deepspeech->model_state, &deepspeech->streaming_state);
		if (status != DS_ERR_OK) {
			GST_ELEMENT_ERROR (GST_ELEMENT(deepspeech), LIBRARY, SETTINGS,
				          ("%s", DS_ErrorCodeToErrorMessage(status)),
				          (NULL));
		}
	}
}

static GstStateChangeReturn
gst_ptdeepspeech_change_state (GstElement     *element,
                               GstStateChange  transition)
{
	GstPtDeepspeech *deepspeech = GST_PTDEEPSPEECH (element);
	GstStateChangeReturn ret;

	/* handle upward state changes */
	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		if (!gst_ptdeepspeech_load_model (deepspeech))
			return GST_STATE_CHANGE_FAILURE;
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS(gst_ptdeepspeech_parent_class)->change_state(element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	/* handle downward state changes */
	switch (transition) {
	case GST_STATE_CHANGE_READY_TO_NULL:
		DS_FreeModel (deepspeech->model_state);
		break;
	default:
		break;
	}

	return ret;
}

static GstFlowReturn
gst_ptdeepspeech_chain (GstPad    *pad,
                        GstObject *parent,
                        GstBuffer *buf)
{
	GstPtDeepspeech *deepspeech = GST_PTDEEPSPEECH (parent);

	GstMapInfo info;
	int        frame_type;
	gboolean   final = FALSE;

	if (GST_STATE (GST_ELEMENT (deepspeech)) != GST_STATE_PLAYING)
		return gst_pad_push (deepspeech->srcpad, buf);

	gst_buffer_map(buf, &info, GST_MAP_READ);
	frame_type = vad_update (deepspeech->vad, (gint16 *)info.data, info.size / sizeof (gint16));

	if (frame_type == VAD_SILENCE && !deepspeech->silence_detected) {
		deepspeech->consecutive_silence_buffers++;
		if (GST_BUFFER_DURATION_IS_VALID (buf)) {
			deepspeech->consecutive_silence_time += buf->duration;
		} else {
			GST_WARNING
			("Invalid buffer duration, consecutive_silence_time update not possible");
		}

		if (deepspeech->consecutive_silence_time >= deepspeech->minimum_silence_time) {
			deepspeech->silence_detected = TRUE;
			final = TRUE;
		}
	}

	if (frame_type != VAD_SILENCE) {
		deepspeech->consecutive_silence_buffers = 0;
		deepspeech->consecutive_silence_time = 0;
		deepspeech->silence_detected = FALSE;
	}

	DS_FeedAudioContent (deepspeech->streaming_state,
	                    (const short *) info.data,
	                    (unsigned int) info.size/sizeof(short));

	if (final) {
		get_text (deepspeech, buf, FINAL);
		deepspeech->last_result_time = GST_BUFFER_TIMESTAMP (buf);
	} else {
		if (!deepspeech->silence_detected &&
		    (GST_BUFFER_TIMESTAMP (buf) - deepspeech->last_result_time) > GST_MSECOND * 500) {
			get_text (deepspeech, buf, INTERMEDIATE);
			deepspeech->last_result_time = GST_BUFFER_TIMESTAMP (buf);
		}
	}

	return gst_pad_push (deepspeech->srcpad, buf);
}

static gboolean
gst_ptdeepspeech_event (GstPad    *pad,
                        GstObject *parent,
                        GstEvent  *event)
{
	GstPtDeepspeech *deepspeech = GST_PTDEEPSPEECH (parent);
	gboolean ret;

	GST_LOG_OBJECT (deepspeech, "Received %s event: %" GST_PTR_FORMAT,
		GST_EVENT_TYPE_NAME (event), event);

	switch (GST_EVENT_TYPE (event)) {
	case GST_EVENT_CAPS:;
		GstCaps * caps;

		gst_event_parse_caps (event, &caps);
		ret = gst_pad_event_default (pad, parent, event);
		break;
	case GST_EVENT_EOS:
		get_text (deepspeech, NULL, FINAL);
		ret = gst_pad_event_default (pad, parent, event);
		break;
	default:
		ret = gst_pad_event_default (pad, parent, event);
	}
	return ret;
}

static void
gst_ptdeepspeech_finalize (GObject *object)
{
	GstPtDeepspeech *deepspeech = GST_PTDEEPSPEECH (object);

	vad_destroy (deepspeech->vad);
	deepspeech->vad = NULL;

	g_free (deepspeech->speech_model_path);
	g_free (deepspeech->scorer_path);

	G_OBJECT_CLASS (gst_ptdeepspeech_parent_class)->finalize(object);
}

static void
gst_ptdeepspeech_init (GstPtDeepspeech *deepspeech)
{
	deepspeech->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
	gst_pad_set_event_function (deepspeech->sinkpad,
	                            GST_DEBUG_FUNCPTR(gst_ptdeepspeech_event));
	gst_pad_set_chain_function (deepspeech->sinkpad,
	                            GST_DEBUG_FUNCPTR(gst_ptdeepspeech_chain));
	GST_PAD_SET_PROXY_CAPS (deepspeech->sinkpad);
	gst_element_add_pad (GST_ELEMENT (deepspeech), deepspeech->sinkpad);

	deepspeech->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
	GST_PAD_SET_PROXY_CAPS (deepspeech->srcpad);
	gst_element_add_pad (GST_ELEMENT (deepspeech), deepspeech->srcpad);

	deepspeech->vad = vad_new (DEFAULT_VAD_HYSTERESIS, DEFAULT_VAD_THRESHOLD);
	deepspeech->silent = TRUE;
	deepspeech->consecutive_silence_buffers = 0;
	deepspeech->consecutive_silence_time = 0;
	deepspeech->minimum_silence_buffers = 0;
	deepspeech->minimum_silence_time = 100;
	deepspeech->last_result_time = GST_CLOCK_TIME_NONE;
}

static void
gst_ptdeepspeech_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
	GstPtDeepspeech *deepspeech = GST_PTDEEPSPEECH (object);

	switch (prop_id) {
	case PROP_SPEECH_MODEL:
		g_free (deepspeech->speech_model_path);
		deepspeech->speech_model_path = g_value_dup_string (value);
		break;
	case PROP_SCORER:
		g_free (deepspeech->scorer_path);
		deepspeech->scorer_path = g_value_dup_string (value);
		break;
	case PROP_BEAM_WIDTH:
		deepspeech->beam_width = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_ptdeepspeech_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
	GstPtDeepspeech *deepspeech = GST_PTDEEPSPEECH (object);

	switch (prop_id) {
	case PROP_SPEECH_MODEL:
		g_value_set_string (value, deepspeech->speech_model_path);
		break;
	case PROP_SCORER:
		g_value_set_string (value, deepspeech->scorer_path);
		break;
	case PROP_BEAM_WIDTH:
		g_value_set_int (value, deepspeech->beam_width);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_ptdeepspeech_class_init (GstPtDeepspeechClass *klass)
{
	GObjectClass    *object_class  = (GObjectClass *) klass;
	GstElementClass *element_class = (GstElementClass *) klass;;

	object_class->set_property = gst_ptdeepspeech_set_property;
	object_class->get_property = gst_ptdeepspeech_get_property;
	object_class->finalize     = gst_ptdeepspeech_finalize;

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
			G_PARAM_READWRITE);

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

	if (!gst_element_register(plugin,
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
