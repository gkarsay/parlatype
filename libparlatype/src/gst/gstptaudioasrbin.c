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
 * gstptaudioasrbin
 * Child bin (GstBin) for parent PtAudioBin.
 *
 * This bin is for Automatic Speech Recognition (ASR).
 *
 *  .-------------------------------------------------------.
 *  | pt_audio_asr_bin                                      |
 *  | .---------.   .----------.   .---------.   .--------. |
 *  | | audio-  |   | audio-   |   | caps-   |   | app-   | |
 *  --> convert ----> resample ----> filter  ----> sink   | |
 *  | '---------'   '----------'   '---------'   '--------' |
 *  '-------------------------------------------------------'
 *
 */

#define GETTEXT_PACKAGE GETTEXT_LIB

#include "config.h"

#include "gstptaudioasrbin.h"

#include "gst-helpers.h"
#include "pt-config.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gst/audio/audio.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_asr_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_asr_bin_debug

#define parent_class gst_pt_audio_asr_bin_parent_class

struct _GstPtAudioAsrBin
{
  GstBin parent;

  GstElement *appsink;
};

G_DEFINE_TYPE (GstPtAudioAsrBin, gst_pt_audio_asr_bin, GST_TYPE_BIN);

GstElement *
gst_pt_audio_asr_bin_get_appsink (GstPtAudioAsrBin *self)
{
  return self->appsink;
}

static void
gst_pt_audio_asr_bin_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_pt_audio_asr_bin_init (GstPtAudioAsrBin *self)
{
  GstElement *audioconvert, *audioresample;
  GstElement *queue;
  GstPad     *queue_sink;

  queue = _pt_make_element ("queue",
                            "queue", NULL);
  audioconvert = _pt_make_element ("audioconvert",
                                   "audioconvert", NULL);
  audioresample = _pt_make_element ("audioresample",
                                    "audioresample", NULL);
  self->appsink = _pt_make_element ("appsink",
                                    "appsink", NULL);

  // g_object_set (queue, "leaky", 2, NULL);
  g_object_set (self->appsink, "emit-signals", TRUE, "sync", FALSE, NULL);

  /* create audio output */
  gst_bin_add_many (GST_BIN (self), queue, audioconvert, audioresample, self->appsink, NULL);
  if (!gst_element_link_many (queue, audioconvert, audioresample, self->appsink, NULL))
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                        "MESSAGE", "ptaudioasrbin: elements not linked");
    }

  /* create ghost pad for audiosink */
  queue_sink = gst_element_get_static_pad (queue, "sink");
  gst_element_add_pad (GST_ELEMENT (self),
                       gst_ghost_pad_new ("sink", queue_sink));
  gst_object_unref (GST_OBJECT (queue_sink));
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
      PACKAGE_URL);
}
