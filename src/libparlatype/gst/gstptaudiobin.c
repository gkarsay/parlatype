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
 * gstptaudiobin:
 * Audio bin (GstBin) for Parlatype.
 *
 * An audio bin that can be connected to a playbin element via playbin's
 * audio-filter property. It supports normal playback and silent ASR output
 * using an ASR plugin.
 *
 * ```
 *  .---------------------------------.
 *  | pt_audio_bin                    |
 *  | .------.  .-------------------. |
 *  | | tee  |  | pt_audio_play_bin | |
 * --->      --->                   | |
 *  | |      |  | sink: pulsesink   | |
 *  | |      |  '-------------------' |
 *  | |      |  .-------------------. |
 *  | |      |  | pt_audio_asr_bin  | |
 *  | |      --->                   | |
 *  | |      |  | sink: gstappsink  | |
 *  | '------'  '-------------------' |
 *  '---------------------------------'
 * ```
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
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_bin_debug

#define parent_class gst_pt_audio_bin_parent_class

struct _GstPtAudioBin
{
  GstBin     parent;
  PtModeType mode;

  GstElement *tee;
  GstElement *play_bin;
  GstElement *asr_bin;
  GstPad     *tee_play_src;
  GstPad     *tee_asr_src;
  GstPad     *queue_src_play;
  GstPad     *queue_src_asr;

  /* properties */
  gboolean player;
  gboolean asr;
  gfloat   volume;
  gboolean mute;

  GstElement *audiosink;
};

G_DEFINE_TYPE (GstPtAudioBin, gst_pt_audio_bin, GST_TYPE_BIN);

PtModeType
gst_pt_audio_bin_get_mode (GstPtAudioBin *self)
{
  g_return_val_if_fail (GST_IS_PT_AUDIO_BIN (self), PT_MODE_INVALID);

  return self->mode;
}

gboolean
gst_pt_audio_bin_set_mode (GstPtAudioBin *self,
                           PtModeType     mode)
{
  GstPad          *blockpad;
  GstElement      *blocked_element;
  GstPad          *new_pad;
  GstElement      *new_element;
  GstPad          *sinkpad;
  gboolean         success;
  GstPadLinkReturn r;

  g_return_val_if_fail (GST_IS_PT_AUDIO_BIN (self), FALSE);

  if (self->mode == mode)
    {
      return TRUE;
    }

  switch (mode)
    {
    case (PT_MODE_PLAYBACK):
      blockpad = self->tee_asr_src;
      blocked_element = self->asr_bin;
      new_pad = self->tee_play_src;
      new_element = self->play_bin;

      break;
    case (PT_MODE_ASR):
      blockpad = self->tee_play_src;
      blocked_element = self->play_bin;
      new_pad = self->tee_asr_src;
      new_element = self->asr_bin;
      break;
    default:
      g_return_val_if_reached (FALSE);
    }

  /* Unlink upstream, i.e. child's sink pad */
  GST_DEBUG_OBJECT (blocked_element, "unlinking");
  sinkpad = gst_element_get_static_pad (blocked_element, "sink");
  success = gst_pad_unlink (blockpad, sinkpad);
  g_object_unref (sinkpad);
  g_assert (success);

  // success = gst_pad_send_event (sinkpad, gst_event_new_eos ());
  // GST_DEBUG_OBJECT (sinkpad, "eos event %s", success ? "sent" : "not sent");

  /* set to null or ready */
  gst_element_set_state (blocked_element, GST_STATE_NULL);

  /* remove from bin, dereferences removed element, we want to keep it */
  GST_DEBUG_OBJECT (blocked_element, "removing");
  gst_object_ref (blocked_element);
  gst_bin_remove (GST_BIN (self), blocked_element);

  GST_DEBUG_OBJECT (new_element, "adding");
  success = gst_bin_add (GST_BIN (self), new_element);
  g_assert (success);
  success = gst_element_sync_state_with_parent (new_element);
  g_assert (success);
  success = gst_bin_sync_children_states (GST_BIN (new_element));
  g_assert (success);

  GST_DEBUG_OBJECT (new_element, "linking");
  sinkpad = gst_element_get_static_pad (new_element, "sink");
  r = _pt_pad_link (new_pad, sinkpad);
  gst_object_unref (sinkpad);
  g_assert (r == GST_PAD_LINK_OK);

  GST_DEBUG_OBJECT (self, "switched mode to \"%s\"", pt_mode_get_name (mode));
  self->mode = mode;
  return TRUE;
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

GstElement *
gst_pt_audio_bin_get_appsink (GstPtAudioBin *self)
{
  g_return_val_if_fail (GST_IS_PT_AUDIO_BIN (self), NULL);

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
gst_pt_audio_bin_constructed (GObject *object)
{
  GstPtAudioBin *self = GST_PT_AUDIO_BIN (object);
  GstPad        *tee_sink;
  GstPad        *play_sink;

  self->tee = _pt_make_element ("tee", "tee", NULL);
  self->play_bin = _pt_make_element ("ptaudioplaybin", "play-audiobin", NULL);
  self->asr_bin = _pt_make_element ("ptaudioasrbin", "asr-audiobin", NULL);

  gst_bin_add_many (GST_BIN (self), self->tee, self->play_bin, NULL);

  /* Play branch */
  self->tee_play_src = gst_element_request_pad_simple (self->tee, "src_1");
  play_sink = gst_element_get_static_pad (self->play_bin, "sink");
  _pt_pad_link (self->tee_play_src, play_sink);

  /* ASR branch */
  self->tee_asr_src = gst_element_request_pad_simple (self->tee, "src_2");

  /*g_object_set (self->tee, "allow-not-linked", TRUE, NULL);
  gst_pad_add_probe (self->tee_asr_src,
                     GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_BUFFER,
                     block_probe_cb, self, NULL);*/
  self->mode = PT_MODE_PLAYBACK;

  /* create ghost pad for audiosink */
  tee_sink = gst_element_get_static_pad (self->tee, "sink");
  gst_element_add_pad (GST_ELEMENT (self),
                       gst_ghost_pad_new ("sink", tee_sink));
  gst_object_unref (tee_sink);

  gst_object_unref (GST_OBJECT (play_sink));
}

static void
gst_pt_audio_bin_init (GstPtAudioBin *self)
{
  gst_pt_audio_play_bin_register ();
  gst_pt_audio_asr_bin_register ();
}

static void
gst_pt_audio_bin_class_init (GstPtAudioBinClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gst_pt_audio_bin_constructed;
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
