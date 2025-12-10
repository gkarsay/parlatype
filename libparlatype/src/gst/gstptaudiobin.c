/* Copyright 2020 Gabor Karsay <gabor.karsay@gmx.at>
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
 *
 * .---------------------------------------------.
 * | pt_audio_bin                                |
 * | .------.  .--------.  .-------------------. |
 * | | tee  |  |        |  | pt_audio_play_bin | |
 * -->      --->        --->                   | |
 * | |      |  | multi- |  |                   | |
 * | |      |  | queue  |  '-------------------' |
 * | |      |  |        |  .-------------------. |
 * | |      |  |        |  | pt_audio_asr_bin  | |
 * | |      --->        --->                   | |
 * | |      |  |        |  |                   | |
 * | '------'  '--------'  '-------------------' |
 * '---------------------------------------------'
 */

#define GETTEXT_PACKAGE GETTEXT_LIB

#include "config.h"

#include "gstptaudiobin.h"

#include "gst-helpers.h"
#include "gstptaudioasrbin.h"
#include "gstptaudioplaybin.h"
#include "pt-config.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_bin_debug

#define parent_class gst_pt_audio_bin_parent_class

struct _GstPtAudioBin
{
  GstBin     parent;
  PtModeType mode;
  PtModeType pending;

  GstElement *play_bin;
  GstElement *asr_bin;
  GstPad     *tee_sink;
  GstPad     *tee_play_src;
  GstPad     *tee_asr_src;
  GstElement *queue;
  GstPad     *queue_src_play;
  GstPad     *queue_src_asr;

  /* properties */
  gboolean player;
  gboolean asr;
  gfloat   volume;
  gboolean mute;
};

G_DEFINE_TYPE (GstPtAudioBin, gst_pt_audio_bin, GST_TYPE_BIN);

static GstPadProbeReturn
change_mode_cb (GstPad          *pad,
                GstPadProbeInfo *info,
                gpointer         user_data)
{
  GstPtAudioBin   *self = GST_PT_AUDIO_BIN (user_data);
  GstElement      *child;
  GstPad          *sinkpad;
  GstPad          *tee_src;
  GstObject       *parent;
  GstPadLinkReturn r;
  gboolean         success;

  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

  /* get element to remove */
  switch (self->pending)
    {
    case (PT_MODE_PLAYBACK):
      child = self->asr_bin;
      break;
    case (PT_MODE_ASR):
      child = self->play_bin;
      break;
    default:
      g_return_val_if_reached (GST_PAD_PROBE_OK);
    }

  parent = gst_element_get_parent (child);
  if (!parent)
    {
      GST_DEBUG_OBJECT (child, "%s has no parent", GST_OBJECT_NAME (child));
      return GST_PAD_PROBE_OK;
    }

  /* Unlink upstream, i.e. child's sink pad */
  sinkpad = gst_element_get_static_pad (child, "sink");
  GST_DEBUG_OBJECT (child, "unlinking %s", GST_OBJECT_NAME (child));
  success = gst_pad_unlink (pad, sinkpad);
  g_assert (success);

  /* TODO investigate if flushing necessary */
  success = gst_pad_send_event (sinkpad, gst_event_new_flush_start ());
  GST_DEBUG_OBJECT (self, "flush-start event %s", success ? "sent" : "not sent");
  success = gst_pad_send_event (sinkpad, gst_event_new_flush_stop (TRUE));
  GST_DEBUG_OBJECT (self, "flush-stop event %s", success ? "sent" : "not sent");

  /* set to null or ready */
  gst_element_set_state (child, GST_STATE_NULL);

  /* remove from bin, dereferences removed element, we want to keep it */
  GST_DEBUG_OBJECT (child, "removing %s from %s", GST_OBJECT_NAME (child), GST_OBJECT_NAME (parent));
  gst_object_ref (child);
  gst_bin_remove (GST_BIN (parent), child);

  gst_object_unref (parent);
  g_object_unref (sinkpad);

  /* get element to link */
  switch (self->pending)
    {
    case (PT_MODE_PLAYBACK):
      child = self->play_bin;
      tee_src = self->tee_play_src;
      break;
    case (PT_MODE_ASR):
      child = self->asr_bin;
      tee_src = self->tee_asr_src;
      break;
    default:
      g_return_val_if_reached (GST_PAD_PROBE_OK);
    }

  /* check if in bin */
  parent = gst_element_get_parent (child);
  if (parent)
    {
      GST_DEBUG_OBJECT (child, "%s has already a parent %s",
                        GST_OBJECT_NAME (child), GST_OBJECT_NAME (parent));
      gst_object_unref (parent);
      return GST_PAD_PROBE_OK;
    }

  GST_DEBUG_OBJECT (child, "adding %s to %s",
                    GST_OBJECT_NAME (child), GST_OBJECT_NAME (self));
  success = gst_bin_add (GST_BIN (self), child);
  g_assert (success);
  success = gst_element_sync_state_with_parent (child);
  g_assert (success);
  success = gst_bin_sync_children_states (GST_BIN (child));
  g_assert (success);

  sinkpad = gst_element_get_static_pad (child, "sink");
  r = gst_pad_link (tee_src, sinkpad);
  g_assert (r == GST_PAD_LINK_OK);

  gst_object_unref (sinkpad);
  GST_DEBUG_OBJECT (self, "switched mode to %d", self->pending);
  self->mode = self->pending;

  return GST_PAD_PROBE_OK;
}

PtModeType
gst_pt_audio_bin_get_mode (GstPtAudioBin *self)
{
  return self->pending;
}

void
gst_pt_audio_bin_set_mode (GstPtAudioBin *self,
                           PtModeType new)
{
  GstPad *blockpad;

  if (self->pending == new)
    return;

  self->pending = new;

  /*if (self->pending == PT_MODE_ASR &&
      !GST_PT_AUDIO_ASR_BIN (self->asr_bin)->is_configured)
    return;*/
  /* FIXME return FALSE, error */

  switch (self->pending)
    {
    case (PT_MODE_PLAYBACK):
      blockpad = self->tee_asr_src;
      break;
    case (PT_MODE_ASR):
      blockpad = self->tee_play_src;
      break;
    default:
      g_return_if_reached ();
    }

  GST_DEBUG_OBJECT (self, "blocking pad for mode switch from %d to %d",
                    self->mode, self->pending);

  gst_pad_add_probe (blockpad,
                     GST_PAD_PROBE_TYPE_BLOCKING | GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                     change_mode_cb, self, NULL);

  /* Pad is blocked. Actual mode switch will happen when going to PLAYING state. */
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

GstElement*
gst_pt_audio_bin_get_appsink (GstPtAudioBin *self)
{
  return gst_pt_audio_asr_bin_get_appsink (GST_PT_AUDIO_ASR_BIN (self->asr_bin));
}

static void
gst_pt_audio_bin_dispose (GObject *object)
{
  GstPtAudioBin *self = GST_PT_AUDIO_BIN (object);

  if (self->play_bin)
    unref_element_without_parent (self->play_bin);
  if (self->asr_bin)
    unref_element_without_parent (self->asr_bin);
  if (self->tee_play_src)
    gst_object_unref (GST_OBJECT (self->tee_play_src));
  if (self->tee_asr_src)
    gst_object_unref (GST_OBJECT (self->tee_asr_src));

  G_OBJECT_CLASS (gst_pt_audio_bin_parent_class)->dispose (object);
}

static void
gst_pt_audio_bin_init (GstPtAudioBin *self)
{
  GstElement *tee;
  // GstPad     *tee_sink;
  GstPad *tee_src1;
  GstPad *tee_src2;
  GstPad *queue_sink1;
  GstPad *queue_sink2;
  GstPad *play_sink;

  gst_pt_audio_play_bin_register ();
  gst_pt_audio_asr_bin_register ();

  tee = _pt_make_element ("tee", "tee", NULL);
  self->play_bin = _pt_make_element ("ptaudioplaybin", "play-audiobin", NULL);
  self->asr_bin = _pt_make_element ("ptaudioasrbin", "asr-audiobin", NULL);
  self->queue = _pt_make_element ("multiqueue", "multiqueue", NULL);

  gst_bin_add_many (GST_BIN (self), tee, self->queue, self->play_bin, NULL);

  /* Pads */
  self->tee_sink = gst_element_get_static_pad (tee, "sink");
  tee_src1 = gst_element_request_pad_simple (tee, "src_%u");
  tee_src2 = gst_element_request_pad_simple (tee, "src_%u");
  queue_sink1 = gst_element_request_pad_simple (self->queue, "sink_1");
  queue_sink2 = gst_element_request_pad_simple (self->queue, "sink_2");
  self->queue_src_play = gst_element_get_static_pad (self->queue, "src_1");
  self->queue_src_asr = gst_element_get_static_pad (self->queue, "src_2");
  self->tee_play_src = gst_element_request_pad_simple (tee, "src_%u");
  self->tee_asr_src = gst_element_request_pad_simple (tee, "src_%u");
  play_sink = gst_element_get_static_pad (self->play_bin, "sink");

  /* link tee with multiqueue */
  gst_pad_link (tee_src1, queue_sink1);
  gst_pad_link (tee_src2, queue_sink2);

  /* link play bin with tee */
  gst_pad_link (self->tee_play_src, play_sink);
  self->mode = PT_MODE_PLAYBACK;

  /* create ghost pad for audiosink */
  gst_element_add_pad (GST_ELEMENT (self),
                       gst_ghost_pad_new ("sink", self->tee_sink));

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
      PACKAGE_URL);
}
