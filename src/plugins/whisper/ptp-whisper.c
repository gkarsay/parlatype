/* Copyright 2026 Gabor Karsay <gabor.karsay@gmx.at>
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

#define G_LOG_DOMAIN "ptp-whisper"

#include "config.h"

#include "ptp-whisper.h"

#include "contrib/gnome-languages.h"
#include "parlatype.h"

#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>
#include <whisper.h>

struct _PtpWhisper
{
  GObject parent_instance;

  PtPlayer   *player;
  PtTextView *text_view;
  GstElement *appsink;

  /* Whisper */
  struct whisper_context    *whisper_ctx;
  struct whisper_full_params wparams;
  gchar                     *speech_model_path;
  gchar                     *language;
  float                     *buffer;
  uint                       buffer_index;

  /* Voice Activity Detection (VAD) */
  gint    quiet_bufs;
  gdouble silence_threshold;

  guint64      silence_time;
  gint         vad_min_silence;
  gboolean     in_speech;
  GstClockTime last_result_time; /* timestamp of last partial result */

  char *result;
  uint  source_handler;
};

typedef struct
{
  PtpWhisper *self;
  const char *text;
} TextData;

enum
{
  PROP_PLAYER = 1,
  PROP_TEXT_VIEW,
  PROP_LANGUAGE,
  PROP_SPEECH_MODEL,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];
static void        pt_asr_iface_init (PtSpeechRecognizerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (PtpWhisper, ptp_whisper, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (PT_TYPE_SPEECH_RECOGNIZER, pt_asr_iface_init))

static gboolean
ptp_whisper_load_model (PtpWhisper *self)
{
  struct whisper_context_params cparams;
  if (!self->whisper_ctx)
    {
      cparams = whisper_context_default_params ();
      self->whisper_ctx = whisper_init_from_file_with_params (self->speech_model_path, cparams);
    }

  if (!self->whisper_ctx)
    return FALSE;

  self->wparams = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
  self->wparams.language = "de";
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "model is multilingual: %s", whisper_is_multilingual (self->whisper_ctx) ? "yes" : "no");
  g_print ("languages1: %d\n", whisper_lang_max_id ());
  g_print ("languages2: %d\n", whisper_n_vocab (self->whisper_ctx) - 51765 - (whisper_is_multilingual (self->whisper_ctx) ? 1 : 0));
  for (int i = 0; i <= whisper_lang_max_id (); i++)
    {
      g_print ("%d: %s - %s\n", i, whisper_lang_str (i), gnome_get_language_from_locale (whisper_lang_str (i), NULL));
    }

  GtkStringList *lang_list = gtk_string_list_new (NULL);
  for (int i = 0; i <= whisper_lang_max_id (); i++)
    {
      gtk_string_list_take (lang_list, gnome_get_language_from_locale (whisper_lang_str (i), NULL));
    }

  return TRUE;
}

static gboolean
insert_text_cb (TextData *data)
{
  pt_text_view_insert_asr_final (data->self->text_view, (void *) data->text);
  g_slice_free (TextData, data);
  return G_SOURCE_REMOVE;
}

static void
ptp_whisper_get_segment_text (PtpWhisper *self)
{
  const int n_segments = whisper_full_n_segments (self->whisper_ctx);
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "processing %d segments", n_segments);
  for (int i = 0; i < n_segments; ++i)
    {
      const char *text = whisper_full_get_segment_text (self->whisper_ctx, i);
      if (text == NULL || strlen (text) == 0)
        {
          continue;
        }
      TextData *data = g_new (TextData, 1);
      data->self = self;
      data->text = text;
      self->source_handler = g_idle_add (G_SOURCE_FUNC (insert_text_cb), data);
    }
}

static void
ptp_whisper_finalize_utt (PtpWhisper *self)
{
  if (whisper_full_parallel (self->whisper_ctx, self->wparams, self->buffer, self->buffer_index, 1) != 0)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                        "MESSAGE", "failed to process audio");
      return;
    }
  ptp_whisper_get_segment_text (self);
}

static GstFlowReturn
new_sample_cb (GstAppSink *sink,
               gpointer    user_data)
{
  PtpWhisper *self = PTP_WHISPER (user_data);

  gint16    *raw_ptr;
  GstSample *sample = gst_app_sink_pull_sample (sink);
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  GstMapInfo info;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ))
    {
      gst_sample_unref (sample);
      return GST_FLOW_ERROR;
    };

  if (self->buffer_index >= 160000 - info.size * 2)
    {
      if (whisper_full (self->whisper_ctx, self->wparams, self->buffer, self->buffer_index) != 0)
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                            "MESSAGE", "failed to process audio");
          return GST_FLOW_ERROR;
        }
      ptp_whisper_get_segment_text (self);
      self->buffer_index = 0;
    }
  for (uint i = 0; i < info.size / sizeof (gint16); i++)
    {
      raw_ptr = (gint16 *) info.data;
      self->buffer[self->buffer_index] = raw_ptr[i] / 32768.0;
      self->buffer_index += 1;
    }

  gst_buffer_unmap (buffer, &info);
  gst_sample_unref (sample);

  return GST_FLOW_OK;
}

static void
eos_cb (GstAppSink *sink,
        gpointer    user_data)
{
  PtpWhisper *self = PTP_WHISPER (user_data);
  ptp_whisper_finalize_utt (self);

  g_signal_handlers_disconnect_by_func (self->appsink, G_CALLBACK (new_sample_cb), self);
  g_signal_handlers_disconnect_by_func (self->appsink, G_CALLBACK (eos_cb), self);
}

static void
ptp_whisper_load (PtSpeechRecognizer *pt_asr_iface)
{
  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (pt_asr_iface));

  PtpWhisper *self = PTP_WHISPER (pt_asr_iface);
  GstCaps    *caps;

  g_object_get (pt_asr_iface, "player", &self->player, "textview", &self->text_view, NULL);
  self->appsink = pt_player_get_appsink (self->player);
  caps = gst_caps_new_simple ("audio/x-raw",
                              "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
                              "channels", G_TYPE_INT, 1,
                              "rate", G_TYPE_INT, 16000, NULL);
  g_object_set (self->appsink, "caps", caps, NULL);
  gst_caps_unref (caps);

  g_signal_connect (self->appsink, "new-sample", G_CALLBACK (new_sample_cb), self);
  g_signal_connect (self->appsink, "eos", G_CALLBACK (eos_cb), self);

  ptp_whisper_load_model (self);
}

static void
ptp_whisper_unload (PtSpeechRecognizer *self)
{
}

static void
pt_asr_iface_init (PtSpeechRecognizerInterface *iface)
{
  iface->load = ptp_whisper_load;
  iface->unload = ptp_whisper_unload;
}

static void
ptp_whisper_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  PtpWhisper *self = PTP_WHISPER (object);

  switch (property_id)
    {
    case PROP_PLAYER:
      self->player = g_value_get_object (value);
      break;
    case PROP_TEXT_VIEW:
      self->text_view = g_value_get_object (value);
      break;
    case PROP_LANGUAGE:
      g_free (self->speech_model_path);
      self->language = g_value_dup_string (value);
      break;
    case PROP_SPEECH_MODEL:
      g_free (self->speech_model_path);
      self->speech_model_path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ptp_whisper_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  PtpWhisper *self = PTP_WHISPER (object);

  switch (property_id)
    {
    case PROP_PLAYER:
      g_value_set_object (value, self->player);
      break;
    case PROP_TEXT_VIEW:
      g_value_set_object (value, self->text_view);
      break;
    case PROP_LANGUAGE:
      g_value_set_string (value, self->language);
      break;
    case PROP_SPEECH_MODEL:
      g_value_set_string (value, self->speech_model_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ptp_whisper_finalize (GObject *object)
{
  PtpWhisper *self = PTP_WHISPER (object);

  g_free (self->speech_model_path);
  whisper_free (self->whisper_ctx);
  g_free (self->buffer);

  G_OBJECT_CLASS (ptp_whisper_parent_class)->finalize (object);
}

static void
ptp_whisper_init (PtpWhisper *self)
{
  self->in_speech = FALSE;
  self->silence_time = 0;
  self->last_result_time = GST_CLOCK_TIME_NONE;
  self->buffer = g_new0 (float, 160000);
  self->buffer_index = 0;
  ggml_backend_load_all ();
}

static void
ptp_whisper_class_init (PtpWhisperClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = ptp_whisper_set_property;
  G_OBJECT_CLASS (klass)->get_property = ptp_whisper_get_property;
  G_OBJECT_CLASS (klass)->finalize = ptp_whisper_finalize;

  obj_properties[PROP_PLAYER] =
      g_param_spec_object (
          "player", NULL, NULL,
          PT_TYPE_PLAYER,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TEXT_VIEW] =
      g_param_spec_object (
          "textview", NULL, NULL,
          PT_TYPE_TEXT_VIEW,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_LANGUAGE] =
      g_param_spec_string (
          "language", NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SPEECH_MODEL] =
      g_param_spec_string (
          "speech-model", NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}
