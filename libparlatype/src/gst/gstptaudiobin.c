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
 * gstptaudiobin
 * Audio bin (GstBin) for Parlatype.
 *
 * An audio bin that can be connected to a playbin element via playbin's
 * audio-filter property. It supports normal playback and silent ASR output
 * using an ASR plugin.
 * .--------------------------------------.
 * | pt_audio_bin                         |
 * | .-----------.  .-------------------. |
 * | | identity  |  | pt_audio_play_bin | |
 * -->           |  |                   | |
 * | | dynamic   --->                   | |
 * | | linking   |  '-------------------' |
 * | | to 1 of 2 |  .-------------------. |
 * | | sinks     --->                   | |
 * | |           |  | pt_audio_asr_bin  | |
 * | |           |  |                   | |
 * | '-----------'  '-------------------' |
 * '--------------------------------------'

Note 1: It doesn’t work if play_audio_play_bin or pt_audio_asr_bin are added to
        pt_audio_bin but are not linked! Either link it or remove it from bin!

*/

#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include "gst-helpers.h"
#include "gstptaudioasrbin.h"
#include "gstptaudiobin.h"
#include "gstptaudioplaybin.h"
#include "pt-config.h"
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_bin_debug

#define parent_class gst_pt_audio_bin_parent_class

G_DEFINE_TYPE (GstPtAudioBin, gst_pt_audio_bin, GST_TYPE_BIN);


static GstPadProbeReturn
change_mode_cb (GstPad *pad,
                GstPadProbeInfo *info,
                gpointer user_data)
{
  GstPtAudioBin *self = GST_PT_AUDIO_BIN (user_data);
  GstElement *old_child, *new_child;
  GstPad *sinkpad;
  GstObject *parent;
  GstPadLinkReturn r;

  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));
  self->probe_id = 0;

  /* get element to remove */
  switch (self->pending)
    {
    case (PT_MODE_PLAYBACK):
      old_child = self->asr_bin;
      new_child = self->play_bin;
      break;
    case (PT_MODE_ASR):
      old_child = self->play_bin;
      new_child = self->asr_bin;
      break;
    default:
      g_return_val_if_reached (GST_PAD_PROBE_OK);
    }

  parent = gst_element_get_parent (old_child);
  if (!parent)
    {
      GST_DEBUG_OBJECT (old_child, "%s has no parent", GST_OBJECT_NAME (old_child));
      return GST_PAD_PROBE_OK;
    }

  /* Unlink upstream, i.e. child's sink pad */
  sinkpad = gst_element_get_static_pad (old_child, "sink");
  GST_DEBUG_OBJECT (old_child, "unlinking %s", GST_OBJECT_NAME (old_child));
  gst_pad_unlink (pad, sinkpad);

  /* TODO investigate if flushing necessary */

  /* set to null or ready */
  gst_element_set_state (old_child, GST_STATE_NULL);

  /* remove from bin, dereferences removed element, we want to keep it */
  GST_DEBUG_OBJECT (old_child, "removing %s from %s", GST_OBJECT_NAME (old_child), GST_OBJECT_NAME (parent));
  gst_object_ref (old_child);
  gst_bin_remove (GST_BIN (parent), old_child);

  gst_object_unref (parent);
  g_object_unref (sinkpad);

  /* check if in bin */
  parent = gst_element_get_parent (new_child);
  if (parent)
    {
      GST_DEBUG_OBJECT (new_child, "%s has already a parent %s",
                        GST_OBJECT_NAME (new_child), GST_OBJECT_NAME (parent));
      gst_object_unref (parent);
      return GST_PAD_PROBE_OK;
    }

  GST_DEBUG_OBJECT (new_child, "adding %s to %s",
                    GST_OBJECT_NAME (new_child), GST_OBJECT_NAME (self));
  gst_bin_add (GST_BIN (self), new_child);
  GST_DEBUG_OBJECT (new_child, "state: %s",
                    gst_element_state_get_name (GST_STATE (new_child)));

  gst_element_sync_state_with_parent (new_child);
  GST_DEBUG_OBJECT (new_child, "state: %s",
                    gst_element_state_get_name (GST_STATE (new_child)));

  sinkpad = gst_element_get_static_pad (new_child, "sink");
  r = gst_pad_link (self->id_src, sinkpad);
  g_assert (r == GST_PAD_LINK_OK);

  gst_object_unref (sinkpad);
  GST_DEBUG_OBJECT (self, "switched mode to %d", self->pending);
  self->mode = self->pending;

  return GST_PAD_PROBE_OK;
}

typedef struct
{
  GAsyncResult *res;
  GMainLoop *loop;
} SyncData;

static void
quit_loop_cb (GstPtAudioBin *self,
              GAsyncResult *res,
              gpointer user_data)
{
  SyncData *data = user_data;
  data->res = g_object_ref (res);
  g_main_loop_quit (data->loop);
}

gboolean
gst_pt_audio_bin_configure_asr (GstPtAudioBin *self,
                                PtConfig *config,
                                GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  GstPtAudioAsrBin *bin;
  SyncData data;
  GMainContext *context;
  gboolean result;

  bin = GST_PT_AUDIO_ASR_BIN (self->asr_bin);
  context = g_main_context_new ();
  g_main_context_push_thread_default (context);

  data.loop = g_main_loop_new (context, FALSE);
  data.res = NULL;

  gst_pt_audio_asr_bin_configure_asr_async (bin, config, NULL, (GAsyncReadyCallback) quit_loop_cb, &data);
  g_main_loop_run (data.loop);

  result = gst_pt_audio_asr_bin_configure_asr_finish (bin, data.res, error);
  GST_DEBUG_OBJECT (self, "Finished asr configuration");
  GST_DEBUG_OBJECT (self, "asr state: %s", gst_element_state_get_name (GST_STATE (GST_ELEMENT (bin))));

  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);
  g_main_loop_unref (data.loop);
  g_object_unref (data.res);

  return result;
}

PtModeType
gst_pt_audio_bin_get_mode (GstPtAudioBin *self)
{
  return self->mode;
}

void
gst_pt_audio_bin_set_mode (GstPtAudioBin *self,
                           PtModeType new)
{
  GstState state, pending;

  /* Already in requested mode, bail out */
  if (self->mode == new &&
      self->pending == new)
    return;

  /* Already in requested mode, but have to cancel a pending change */
  if (self->mode == new &&
      self->pending != new &&
      self->probe_id > 0)
    {
      gst_pad_remove_probe (self->id_src, self->probe_id);
      self->probe_id = 0;
      GST_DEBUG_OBJECT (self, "cancelled switch from %d to %d",
                        self->mode, self->pending);
      self->pending = new;
      return;
    }

  self->pending = new;

  if (self->pending == PT_MODE_ASR &&
      !GST_PT_AUDIO_ASR_BIN (self->asr_bin)->is_configured)
    return;

  GST_DEBUG_OBJECT (self, "blocking pad for mode switch from %d to %d",
                    self->mode, self->pending);

  /* Wait for any pending state change. Prevents internal stream error
   * (unlinked element) when going to PLAYBACK and pt_player_pause() was
   * called just before. */
  gst_element_get_state (
      GST_ELEMENT (self),
      &state, &pending,
      GST_CLOCK_TIME_NONE);

  GST_DEBUG_OBJECT (self, "state: %s",
                    gst_element_state_get_name (state));
  GST_DEBUG_OBJECT (self, "pending: %s",
                    gst_element_state_get_name (pending));

  self->probe_id = gst_pad_add_probe (self->id_src,
                                      GST_PAD_PROBE_TYPE_BLOCKING |
                                          GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                                      change_mode_cb, self, NULL);
}

static void
unref_element_without_parent (GstElement *element)
{
  GstObject *parent;

  parent = gst_element_get_parent (element);
  if (!parent)
    {
      gst_element_set_state (element, GST_STATE_NULL);
      gst_object_unref (element);
    }
  else
    {
      gst_object_unref (parent);
    }
  element = NULL;
}

static void
gst_pt_audio_bin_dispose (GObject *object)
{
  GstPtAudioBin *self = GST_PT_AUDIO_BIN (object);

  if (self->play_bin)
    unref_element_without_parent (self->play_bin);
  if (self->asr_bin)
    unref_element_without_parent (self->asr_bin);
  if (self->id_sink)
    gst_object_unref (GST_OBJECT (self->id_sink));
  if (self->id_src)
    gst_object_unref (GST_OBJECT (self->id_src));

  G_OBJECT_CLASS (gst_pt_audio_bin_parent_class)->dispose (object);
}

static void
gst_pt_audio_bin_init (GstPtAudioBin *self)
{
  GstElement *identity;
  GstPad *play_sink;

  gst_pt_audio_play_bin_register ();
  gst_pt_audio_asr_bin_register ();

  identity = _pt_make_element ("identity", "identity", NULL);
  self->play_bin = _pt_make_element ("ptaudioplaybin", "play-audiobin", NULL);
  self->asr_bin = _pt_make_element ("ptaudioasrbin", "asr-audiobin", NULL);

  gst_bin_add_many (GST_BIN (self), identity, self->play_bin, NULL);

  self->id_sink = gst_element_get_static_pad (identity, "sink");
  self->id_src = gst_element_get_static_pad (identity, "src");
  play_sink = gst_element_get_static_pad (self->play_bin, "sink");

  /* link play bin with identity */
  gst_pad_link (self->id_src, play_sink);
  self->mode = PT_MODE_PLAYBACK;
  self->pending = PT_MODE_PLAYBACK;
  self->probe_id = 0;

  /* create ghost pad for audiosink */
  gst_element_add_pad (GST_ELEMENT (self),
                       gst_ghost_pad_new ("sink", self->id_sink));

  gst_object_unref (GST_OBJECT (play_sink));
}

static void
gst_pt_audio_bin_class_init (GstPtAudioBinClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = gst_pt_audio_bin_dispose;
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_pt_audio_bin_debug, "ptaudiobin", 0,
                           "Audio bin for Parlatype");

  return (gst_element_register (plugin, "ptaudiobin",
                                GST_RANK_NONE, GST_TYPE_PT_AUDIO_BIN));
}

/**
 * gst_pt_audio_bin_register:
 *
 * Registers a plugin holding our single element to use privately in this
 * library.
 *
 * Return value: TRUE if successful, otherwise FALSE
 */
gboolean
gst_pt_audio_bin_register (void)
{
  return gst_plugin_register_static (
      GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "ptaudiobin",
      "Audio bin for Parlatype",
      plugin_init,
      PACKAGE_VERSION,
      "GPL",
      "libparlatype",
      "Parlatype",
      "https://www.parlatype.org/");
}
