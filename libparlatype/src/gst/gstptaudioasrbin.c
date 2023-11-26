/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
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

/**
 * gstptaudioasrbin
 * Child bin (GstBin) for parent PtAudioBin.
 *
 * This bin is for Automatic Speech Recognition (ASR).
 *
 *  .--------------------------------------------------------.
 *  | pt_audio_asr_bin                                       |
 *  | .---------.   .----------.   .--------.   .----------. |
 *  | | audio-  |   | audio-   |   | ASR    |   | fakesink | |
 *  --> convert ----> resample ----> Plugin ---->          | |
 *  | '---------'   '----------'   '--------'   '----------' |
 *  '--------------------------------------------------------'
 *
 */

#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include "pt-config.h"
#include "gst-helpers.h"
#include "gstptaudioasrbin.h"

GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_asr_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_asr_bin_debug

#define parent_class gst_pt_audio_asr_bin_parent_class

G_DEFINE_TYPE (GstPtAudioAsrBin, gst_pt_audio_asr_bin, GST_TYPE_BIN);

gboolean
gst_pt_audio_asr_bin_is_configured (GstPtAudioAsrBin *self)
{
  return self->is_configured;
}

gboolean
gst_pt_audio_asr_bin_configure_asr_finish (GstPtAudioAsrBin *self,
                                           GAsyncResult *result,
                                           GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
configure_plugin (GTask *task)
{
  GstPtAudioAsrBin *self;
  gchar *plugin;
  gboolean success;
  GError *error = NULL;

  self = g_task_get_source_object (task);
  plugin = pt_config_get_plugin (self->config);

  GST_DEBUG_OBJECT (self, "configuring plugin");

  /* For simplicity always recreate plugin, even if the plugin is the
   * same and only its configuration changed */
  if (self->asr_plugin)
    {
      GST_DEBUG_OBJECT (self, "removing previous plugin");
      /* setting state deadlocks without previous flush-start event */
      gst_element_set_state (self->asr_plugin, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (self), self->asr_plugin);
    }

  GST_DEBUG_OBJECT (self, "creating new plugin %s", plugin);
  self->asr_plugin = _pt_make_element (plugin, plugin, &error);

  if (!self->asr_plugin)
    {
      self->is_configured = FALSE;
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  /* Apply config in NULL state */
  success = pt_config_apply (self->config,
                             G_OBJECT (self->asr_plugin), &error);
  self->is_configured = success;

  if (!success)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }
  gst_element_set_state (self->fakesink, GST_STATE_NULL);

  gst_bin_add (GST_BIN (self), self->asr_plugin);
  gst_element_sync_state_with_parent (self->audioresample);
  gst_element_sync_state_with_parent (self->asr_plugin);
  gst_element_sync_state_with_parent (self->fakesink);

  gst_element_link_many (self->audioresample,
                         self->asr_plugin,
                         self->fakesink, NULL);

  gst_element_sync_state_with_parent (GST_ELEMENT (self));

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static void
flush_plugin (GstPtAudioAsrBin *self)
{
  GstPad *sinkpad;
  gboolean success;

  GST_DEBUG_OBJECT (self, "flushing ASR plugin");
  sinkpad = gst_element_get_static_pad (self->asr_plugin, "sink");
  g_assert (sinkpad != NULL);
  success = gst_pad_send_event (sinkpad, gst_event_new_flush_start ());
  GST_DEBUG_OBJECT (self, "flush-start event %s", success ? "sent" : "not sent");
  gst_object_unref (sinkpad);
}

static GstPadProbeReturn
pad_probe_cb (GstPad *pad,
              GstPadProbeInfo *info,
              gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GstPtAudioAsrBin *self;

  self = g_task_get_source_object (task);
  GST_DEBUG_OBJECT (self, "pad is blocked now");

  /* remove the probe first */
  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

  /* Send flush-start event if plugin exists (EOS event would deadlock
   * in playing state). Don't add event probe, that didn't work before
   * and seems like not necessary, just send event without waiting for
   * it. */
  if (self->asr_plugin)
    {
      flush_plugin (self);
    }

  configure_plugin (task);

  return GST_PAD_PROBE_OK;
}

void
gst_pt_audio_asr_bin_configure_asr_async (GstPtAudioAsrBin *self,
                                          PtConfig *config,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
  GST_DEBUG_OBJECT (self, "configuring asr");

  GTask *task;
  GstPad *blockpad;
  gulong probe_id;
  GstState debug_state;

  task = g_task_new (self, cancellable, callback, user_data);

  /* TODO Compare PtConfigs properly/investigate whether they are still
   * equal if a value has changed */
  if (self->config && self->config == config)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      GST_DEBUG_OBJECT (self, "config didn't change");
      return;
    }

  if (self->config)
    g_object_unref (self->config);
  self->config = g_object_ref (config);

  debug_state = GST_STATE (self->audioresample);
  GST_DEBUG_OBJECT (self, "pad element state: %s",
                    gst_element_state_get_name (debug_state));

  GST_DEBUG_OBJECT (self, "adding probe for blocking pad");
  blockpad = gst_element_get_static_pad (self->audioresample, "src");
  probe_id = gst_pad_add_probe (blockpad,
                                GST_PAD_PROBE_TYPE_BLOCKING |
                                    GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                                pad_probe_cb, task, NULL);
  if (debug_state == GST_STATE_PAUSED)
    {
      gst_pad_remove_probe (blockpad, probe_id);
      if (self->asr_plugin)
        {
          flush_plugin (self);
        }
      configure_plugin (task);
      flush_plugin (self);
    }
  gst_object_unref (blockpad);
}

static void
gst_pt_audio_asr_bin_finalize (GObject *object)
{
  GstPtAudioAsrBin *self = GST_PT_AUDIO_ASR_BIN (object);

  if (self->config)
    g_object_unref (self->config);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_pt_audio_asr_bin_init (GstPtAudioAsrBin *bin)
{
  GstElement *audioconvert;
  GstPad *audioconvert_sink;

  audioconvert = _pt_make_element ("audioconvert",
                                   "audioconvert", NULL);
  bin->audioresample = _pt_make_element ("audioresample",
                                         "audioresample", NULL);
  bin->fakesink = _pt_make_element ("fakesink",
                                    "fakesink", NULL);

  /* create audio output */
  gst_bin_add_many (GST_BIN (bin), audioconvert, bin->audioresample,
                    bin->fakesink, NULL);
  gst_element_link_many (audioconvert, bin->audioresample, NULL);

  /* create ghost pad for audiosink */
  audioconvert_sink = gst_element_get_static_pad (audioconvert, "sink");
  gst_element_add_pad (GST_ELEMENT (bin),
                       gst_ghost_pad_new ("sink", audioconvert_sink));
  gst_object_unref (GST_OBJECT (audioconvert_sink));

  bin->asr_plugin = NULL;
  bin->is_configured = FALSE;
}

static void
gst_pt_audio_asr_bin_class_init (GstPtAudioAsrBinClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gst_pt_audio_asr_bin_finalize;
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_pt_audio_asr_bin_debug,
                           "ptaudioasrbin", 0,
                           "Audio bin for Automatic Speech Recognition "
                           "for Parlatype");

  return (gst_element_register (plugin, "ptaudioasrbin",
                                GST_RANK_NONE,
                                GST_TYPE_PT_AUDIO_ASR_BIN));
}

/**
 * gst_pt_audio_asr_bin_register:
 *
 * Registers a plugin holding our single element to use privately in this
 * library.
 *
 * Return value: TRUE if successful, otherwise FALSE
 */
gboolean
gst_pt_audio_asr_bin_register (void)
{
  return gst_plugin_register_static (
      GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "ptaudioasrbin",
      "Audio bin for Automatic Speech Recognition "
      "for Parlatype",
      plugin_init,
      PACKAGE_VERSION,
      "GPL",
      "libparlatype",
      "Parlatype",
      "https://www.parlatype.org/");
}
