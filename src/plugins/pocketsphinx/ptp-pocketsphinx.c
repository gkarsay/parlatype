#include "config.h"

#include "ptp-pocketsphinx.h"

#include "parlatype.h"

#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>
#include <pocketsphinx.h>
#include <sphinxbase/err.h>
#include <sphinxbase/strfuncs.h>

struct _PtpPocketsphinx
{
  GObject parent_instance;

  PtPlayer   *player;
  PtTextView *text_view;
  GstElement *appsink;

  ps_decoder_t    *ps;
  ps_endpointer_t *ep;
  ps_config_t     *config;

  size_t frame_size;

  gboolean speech_started;
  gboolean listening_started;
  gint     uttno;

  char *last_result; /* String of last partial result. */
  char *result;

  uint source_handler;
  int  counter;
};

enum
{
  PROP_HMM_DIR = 1,
  PROP_LM_FILE,
  PROP_LMCTL_FILE,
  PROP_DICT_FILE,
  PROP_MLLR_FILE,
  PROP_FSG_FILE,
  PROP_ALLPHONE_FILE,
  PROP_KWS_FILE,
  PROP_JSGF_FILE,
  PROP_FWDFLAT,
  PROP_BESTPATH,
  PROP_MAXHMMPF,
  PROP_MAXWPF,
  PROP_BEAM,
  PROP_WBEAM,
  PROP_PBEAM,
  PROP_DSRATIO,
  PROP_LATDIR,
  PROP_LM_NAME,
  PROP_DECODER,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];
static void        pt_asr_iface_init (PtAsrIfaceInterface *iface);

/*
 * Boxing of ps_decoder_t.
 */
GType
ps_decoder_get_type (void)
{
  static GType ps_decoder_type = 0;

  if (G_UNLIKELY (ps_decoder_type == 0))
    {
      ps_decoder_type = g_boxed_type_register_static ("PSDecoder",
                                                      /* Conveniently, these should just work. */
                                                      (GBoxedCopyFunc) ps_retain,
                                                      (GBoxedFreeFunc) ps_free);
    }

  return ps_decoder_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PtpPocketsphinx, ptp_pocketsphinx, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (PT_TYPE_ASR_IFACE, pt_asr_iface_init))

static gboolean
insert_text_cb (PtpPocketsphinx *self)
{
  pt_text_view_insert_asr_final (self->text_view, self->result);
  self->source_handler = 0;
  return G_SOURCE_REMOVE;
}

static void
ptp_pocketsphinx_finalize_utt (PtpPocketsphinx *self)
{
  char const *hyp;
  int32       score;

  hyp = NULL;
  if (!self->listening_started)
    return;

  ps_end_utt (self->ps);
  self->listening_started = FALSE;
  hyp = ps_get_hyp (self->ps, &score);

  if (hyp)
    {
      self->result = hyp;
      self->source_handler = g_idle_add (G_SOURCE_FUNC (insert_text_cb), self);
      /*ptp_pocketsphinx_post_message (self, TRUE, GST_CLOCK_TIME_NONE,
                                    ps_get_prob (self->ps), hyp);*/
    }
}

static void
eos_cb (GstAppSink *sink,
        gpointer    user_data)
{
  PtpPocketsphinx *self = PTP_POCKETSPHINX (user_data);
  ptp_pocketsphinx_finalize_utt (self);
}

static GstFlowReturn
new_sample_cb (GstAppSink *sink,
               gpointer    user_data)
{
  PtpPocketsphinx *self = PTP_POCKETSPHINX (user_data);

  GstSample *sample = gst_app_sink_pull_sample (sink);
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  GstMapInfo info;
  gboolean   in_speech;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ))
    {
      gst_sample_unref (sample);
      return GST_FLOW_ERROR;
    };

  /* Start an utterance for the first buffer we get */
  if (!self->listening_started)
    {
      self->listening_started = TRUE;
      self->speech_started = FALSE;
      ps_start_utt (self->ps);
    }

  ps_process_raw (self->ps,
                  (short *) info.data,
                  info.size / sizeof (short),
                  FALSE, FALSE);

  gst_buffer_unmap (buffer, &info);
  gst_sample_unref (sample);

  in_speech = ps_get_in_speech (self->ps);
  if (in_speech && !self->speech_started)
    {
      self->speech_started = TRUE;
    }
  if (!in_speech && self->speech_started)
    {
      ptp_pocketsphinx_finalize_utt (self);
    }
  /*else if (self->last_result_time == 0
           || (gst_clock_get_time (self->clock) - self->last_result_time) > GST_SECOND)
    {
      int32       score;
      char const *hyp;

      hyp = ps_get_hyp (self->ps, &score);
      self->last_result_time = gst_clock_get_time (self->clock);
      if (hyp && strlen (hyp) > 0)
        {
          if (self->last_result == NULL || 0 != strcmp (self->last_result, hyp))
            {
              g_free (self->last_result);
              self->last_result = g_strdup (hyp);
              ptp_pocketsphinx_post_message (self, FALSE, self->last_result_time,
                                            ps_get_prob (self->ps), hyp);
            }
        }
    }*/

  return GST_FLOW_OK;
}

static void
ptp_pocketsphinx_load (PtAsrIface *pt_asr_iface,
                       PtPlayer   *player,
                       PtTextView *text_view)
{
  g_return_if_fail (PT_IS_ASR_IFACE (pt_asr_iface));
  g_return_if_fail (PT_IS_PLAYER (player));
  g_return_if_fail (PT_IS_TEXT_VIEW (text_view));

  PtpPocketsphinx *self = PTP_POCKETSPHINX (pt_asr_iface);
  GstCaps         *caps;

  self->player = player;
  self->text_view = text_view;
  self->appsink = pt_player_get_appsink (self->player);
  caps = gst_caps_new_simple ("audio/x-raw",
                              "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
                              "channels", G_TYPE_INT, 1,
                              "rate", G_TYPE_INT, 16000, NULL);
  g_object_set (self->appsink, "caps", caps, NULL);
  gst_caps_unref (caps);

  g_signal_connect (self->appsink, "new-sample", G_CALLBACK (new_sample_cb), self);
  g_signal_connect (self->appsink, "eos", G_CALLBACK (eos_cb), self);

  g_print ("%s\n", __func__);
}

static void
ptp_pocketsphinx_unload (PtAsrIface *self)
{
}

static void
pt_asr_iface_init (PtAsrIfaceInterface *iface)
{
  iface->load = ptp_pocketsphinx_load;
  iface->unload = ptp_pocketsphinx_unload;
}

static void
ptp_pocketsphinx_set_string (PtpPocketsphinx *self,
                             const gchar     *key,
                             const GValue    *value)
{
  if (value != NULL)
    {
      ps_config_set_str (self->config, key, g_value_get_string (value));
    }
  else
    {
      ps_config_set_str (self->config, key, NULL);
    }
}

static void
ptp_pocketsphinx_set_int (PtpPocketsphinx *self,
                          const gchar     *key,
                          const GValue    *value)
{
  ps_config_set_int (self->config, key, g_value_get_int (value));
}

static void
ptp_pocketsphinx_set_boolean (PtpPocketsphinx *self,
                              const gchar     *key,
                              const GValue    *value)
{
  ps_config_set_bool (self->config, key, g_value_get_boolean (value));
}

static void
ptp_pocketsphinx_set_double (PtpPocketsphinx *self,
                             const gchar     *key,
                             const GValue    *value)
{
  ps_config_set_float (self->config, key, g_value_get_double (value));
}

static void
ptp_pocketsphinx_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PtpPocketsphinx *self = PTP_POCKETSPHINX (object);

  switch (property_id)
    {

    case PROP_HMM_DIR:
      ptp_pocketsphinx_set_string (self, "hmm", value);
      break;
    case PROP_LM_FILE:
      /* FSG and LM are mutually exclusive. */
      ptp_pocketsphinx_set_string (self, "lm", value);
      ptp_pocketsphinx_set_string (self, "lmctl", NULL);
      ptp_pocketsphinx_set_string (self, "fsg", NULL);
      ptp_pocketsphinx_set_string (self, "allphone", NULL);
      ptp_pocketsphinx_set_string (self, "kws", NULL);
      ptp_pocketsphinx_set_string (self, "jsgf", NULL);
      break;
    case PROP_LMCTL_FILE:
      /* FSG and LM are mutually exclusive. */
      ptp_pocketsphinx_set_string (self, "lm", NULL);
      ptp_pocketsphinx_set_string (self, "lmctl", value);
      ptp_pocketsphinx_set_string (self, "fsg", NULL);
      ptp_pocketsphinx_set_string (self, "allphone", NULL);
      ptp_pocketsphinx_set_string (self, "kws", NULL);
      ptp_pocketsphinx_set_string (self, "jsgf", NULL);
      break;
    case PROP_DICT_FILE:
      ptp_pocketsphinx_set_string (self, "dict", value);
      break;
    case PROP_MLLR_FILE:
      ptp_pocketsphinx_set_string (self, "mllr", value);
      break;
    case PROP_FSG_FILE:
      /* FSG and LM are mutually exclusive */
      ptp_pocketsphinx_set_string (self, "lm", NULL);
      ptp_pocketsphinx_set_string (self, "lmctl", NULL);
      ptp_pocketsphinx_set_string (self, "fsg", value);
      ptp_pocketsphinx_set_string (self, "allphone", NULL);
      ptp_pocketsphinx_set_string (self, "kws", NULL);
      ptp_pocketsphinx_set_string (self, "jsgf", NULL);
      break;
    case PROP_ALLPHONE_FILE:
      /* FSG and LM are mutually exclusive. */
      ptp_pocketsphinx_set_string (self, "lm", NULL);
      ptp_pocketsphinx_set_string (self, "lmctl", NULL);
      ptp_pocketsphinx_set_string (self, "fsg", NULL);
      ptp_pocketsphinx_set_string (self, "allphone", value);
      ptp_pocketsphinx_set_string (self, "kws", NULL);
      ptp_pocketsphinx_set_string (self, "jsgf", NULL);
      break;
    case PROP_KWS_FILE:
      /* FSG and LM are mutually exclusive. */
      ptp_pocketsphinx_set_string (self, "lm", NULL);
      ptp_pocketsphinx_set_string (self, "lmctl", NULL);
      ptp_pocketsphinx_set_string (self, "fsg", NULL);
      ptp_pocketsphinx_set_string (self, "allphone", NULL);
      ptp_pocketsphinx_set_string (self, "jsgf", NULL);
      ptp_pocketsphinx_set_string (self, "kws", value);
      break;
    case PROP_JSGF_FILE:
      /* FSG and LM are mutually exclusive. */
      ptp_pocketsphinx_set_string (self, "lm", NULL);
      ptp_pocketsphinx_set_string (self, "lmctl", NULL);
      ptp_pocketsphinx_set_string (self, "fsg", NULL);
      ptp_pocketsphinx_set_string (self, "allphone", NULL);
      ptp_pocketsphinx_set_string (self, "kws", NULL);
      ptp_pocketsphinx_set_string (self, "jsgf", value);
      break;
    case PROP_FWDFLAT:
      ptp_pocketsphinx_set_boolean (self, "fwdflat", value);
      break;
    case PROP_BESTPATH:
      ptp_pocketsphinx_set_boolean (self, "bestpath", value);
      break;
    case PROP_MAXHMMPF:
      ptp_pocketsphinx_set_int (self, "maxhmmpf", value);
      break;
    case PROP_MAXWPF:
      ptp_pocketsphinx_set_int (self, "maxwpf", value);
      break;
    case PROP_BEAM:
      ptp_pocketsphinx_set_double (self, "beam", value);
      break;
    case PROP_PBEAM:
      ptp_pocketsphinx_set_double (self, "pbeam", value);
      break;
    case PROP_WBEAM:
      ptp_pocketsphinx_set_double (self, "wbeam", value);
      break;
    case PROP_DSRATIO:
      ptp_pocketsphinx_set_int (self, "ds", value);
      break;

    case PROP_LM_NAME:
      ptp_pocketsphinx_set_string (self, "fsg", NULL);
      ptp_pocketsphinx_set_string (self, "lm", NULL);
      ptp_pocketsphinx_set_string (self, "allphone", NULL);
      ptp_pocketsphinx_set_string (self, "kws", NULL);
      ptp_pocketsphinx_set_string (self, "jsgf", NULL);
      ptp_pocketsphinx_set_string (self, "lmname", value);

      /*
       * Chances are that lmctl is already loaded and all
       * corresponding searches are configured, so we simply
       * try to set the search
       */

      if (value != NULL && self->ps)
        {
          ps_activate_search (self->ps, g_value_get_string (value));
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
    }

  /* If decoder was already initialized, reinit */
  if (self->ps && property_id && property_id != PROP_LM_NAME)
    ps_reinit (self->ps, self->config);
}

static void
ptp_pocketsphinx_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PtpPocketsphinx *self = PTP_POCKETSPHINX (object);

  switch (property_id)
    {
    case PROP_DECODER:
      g_value_set_boxed (value, self->ps);
      break;
    case PROP_HMM_DIR:
      g_value_set_string (value, ps_config_str (self->config, "hmm"));
      break;
    case PROP_LM_FILE:
      g_value_set_string (value, ps_config_str (self->config, "lm"));
      break;
    case PROP_LMCTL_FILE:
      g_value_set_string (value, ps_config_str (self->config, "lmctl"));
      break;
    case PROP_LM_NAME:
      g_value_set_string (value, ps_config_str (self->config, "lmname"));
      break;
    case PROP_DICT_FILE:
      g_value_set_string (value, ps_config_str (self->config, "dict"));
      break;
    case PROP_MLLR_FILE:
      g_value_set_string (value, ps_config_str (self->config, "mllr"));
      break;
    case PROP_FSG_FILE:
      g_value_set_string (value, ps_config_str (self->config, "fsg"));
      break;
    case PROP_ALLPHONE_FILE:
      g_value_set_string (value, ps_config_str (self->config, "allphone"));
      break;
    case PROP_KWS_FILE:
      g_value_set_string (value, ps_config_str (self->config, "kws"));
      break;
    case PROP_JSGF_FILE:
      g_value_set_string (value, ps_config_str (self->config, "jsgf"));
      break;
    case PROP_FWDFLAT:
      g_value_set_boolean (value, ps_config_bool (self->config, "fwdflat"));
      break;
    case PROP_BESTPATH:
      g_value_set_boolean (value, ps_config_bool (self->config, "bestpath"));
      break;
    case PROP_MAXHMMPF:
      g_value_set_int (value, ps_config_int (self->config, "maxhmmpf"));
      break;
    case PROP_MAXWPF:
      g_value_set_int (value, ps_config_int (self->config, "maxwpf"));
      break;
    case PROP_BEAM:
      g_value_set_double (value, ps_config_float (self->config, "beam"));
      break;
    case PROP_PBEAM:
      g_value_set_double (value, ps_config_float (self->config, "pbeam"));
      break;
    case PROP_WBEAM:
      g_value_set_double (value, ps_config_float (self->config, "wbeam"));
      break;
    case PROP_DSRATIO:
      g_value_set_int (value, ps_config_int (self->config, "ds"));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ptp_pocketsphinx_finalize (GObject *object)
{
  PtpPocketsphinx *self = PTP_POCKETSPHINX (object);

  ps_free (self->ps);
  ps_config_free (self->config);
  g_free (self->last_result);

  G_OBJECT_CLASS (ptp_pocketsphinx_parent_class)->finalize (object);
}

static void
ptp_pocketsphinx_init (PtpPocketsphinx *self)
{
  /* Parse default command-line options. */
  self->config = ps_config_init (NULL);
  ps_default_search_args (self->config);
  self->ps = ps_init (self->config);
  g_assert (self->ps != NULL);
}

static void
ptp_pocketsphinx_class_init (PtpPocketsphinxClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = ptp_pocketsphinx_set_property;
  G_OBJECT_CLASS (klass)->get_property = ptp_pocketsphinx_get_property;
  G_OBJECT_CLASS (klass)->finalize = ptp_pocketsphinx_finalize;

  obj_properties[PROP_HMM_DIR] =
      g_param_spec_string ("hmm", "HMM Directory",
                           "Directory containing acoustic model parameters",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_LM_FILE] =
      g_param_spec_string ("lm", "LM File",
                           "Language model file",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_LMCTL_FILE] =
      g_param_spec_string ("lmctl", "LM Control File",
                           "Language model control file (for class LMs)",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_DICT_FILE] =
      g_param_spec_string ("dict", "Dictionary File",
                           "Dictionary File",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_MLLR_FILE] =
      g_param_spec_string ("mllr", "MLLR transformation file",
                           "Transformation to apply to means and variances",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_FSG_FILE] =
      g_param_spec_string ("fsg", "FSG File",
                           "Finite state grammar file",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_ALLPHONE_FILE] =
      g_param_spec_string ("allphone", "Allphone File",
                           "Phonetic language model file",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_KWS_FILE] =
      g_param_spec_string ("kws", "Keyphrases File",
                           "List of keyphrases for spotting",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_JSGF_FILE] =
      g_param_spec_string ("jsgf", "Grammar file",
                           "File with grammar in Java Speech Grammar Format (JSGF)",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_FWDFLAT] =
      g_param_spec_boolean ("fwdflat", "Flat Lexicon Search",
                            "Enable Flat Lexicon Search",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_BESTPATH] =
      g_param_spec_boolean ("bestpath", "Graph Search",
                            "Enable Graph Search",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_MAXHMMPF] =
      g_param_spec_int ("maxhmmpf", "Maximum HMMs per frame",
                        "Maximum number of HMMs searched per frame",
                        1, 100000, 1000,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_MAXWPF] =
      g_param_spec_int ("maxwpf", "Maximum words per frame",
                        "Maximum number of words searched per frame",
                        1, 100000, 10,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_BEAM] =
      g_param_spec_double ("beam", "Beam width applied to every frame in Viterbi search",
                           "Beam width applied to every frame in Viterbi search",
                           -1, 1, 1e-48,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_PBEAM] =
      g_param_spec_double ("pbeam", "Beam width applied to phone transitions",
                           "Beam width applied to phone transitions",
                           -1, 1, 1e-48,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_WBEAM] =
      g_param_spec_double ("wbeam", "Beam width applied to word exits",
                           "Beam width applied to phone transitions",
                           -1, 1, 7e-29,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_DSRATIO] =
      g_param_spec_int ("dsratio", "Frame downsampling ratio",
                        "Evaluate acoustic model every N frames",
                        1, 10, 1,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /* Could be changed on runtime when ps is already initialized */
  obj_properties[PROP_LM_NAME] =
      g_param_spec_string ("lmname", "LM Name",
                           "Language model name (to select LMs from lmctl)",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_LATDIR] =
      g_param_spec_string ("latdir", "Lattice Directory",
                           "Output Directory for Lattices",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_DECODER] =
      g_param_spec_boxed ("decoder", "Decoder object",
                          "The underlying decoder",
                          PS_DECODER_TYPE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}
