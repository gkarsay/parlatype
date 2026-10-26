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
 * gstptaudioplaybin:
 * Child bin (GstBin) for parent PtAudioBin.
 *
 * This bin is an audio sink for normal playback.
 * ```
 *  .----------------------------------------------.
 *  | pt_audio_play_bin                            |
 *  | .-----------.  .------------.  .-----------. |
 *  | | queue     |  | capsfilter |  | pulsesink | |
 *  -->           --->            --->           | |
 *  | '-----------'  '------------'  '-----------' |
 *  '----------------------------------------------'
 * ```
 * audiosink is chosen at runtime.
 */

#include "config.h"

#include "gstptaudioplaybin.h"

#include "gst-helpers.h"

#include <gio/gio.h>
#include <gst/audio/streamvolume.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_play_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_play_bin_debug

#define parent_class gst_pt_audio_play_bin_parent_class

struct _GstPtAudioPlayBin
{
  GstBin parent;

  GstElement *audiosink;
};

G_DEFINE_TYPE (GstPtAudioPlayBin, gst_pt_audio_play_bin, GST_TYPE_BIN);

static gboolean
have_pulseaudio_server (void)
{
  /* Adapted from Quod Libet ...quodlibet/player/gstbe/util.py:
   * If we have a pulsesink we can get the server presence through
   * setting the ready state */
  GstElement          *pulse;
  GstStateChangeReturn state;

  pulse = gst_element_factory_make ("pulsesink", NULL);
  if (!pulse)
    return FALSE;
  gst_element_set_state (pulse, GST_STATE_READY);
  state = gst_element_get_state (pulse, NULL, NULL, GST_CLOCK_TIME_NONE);
  gst_element_set_state (pulse, GST_STATE_NULL);
  gst_object_unref (pulse);
  return (state != GST_STATE_CHANGE_FAILURE);
}

GstElement *
gst_pt_audio_play_bin_get_audiosink (GstPtAudioPlayBin *self)
{
  g_return_val_if_fail (GST_IS_PT_AUDIO_PLAY_BIN (self), NULL);

  return self->audiosink;
}

static void
gst_pt_audio_play_bin_constructed (GObject *object)
{
  GstPtAudioPlayBin *self = GST_PT_AUDIO_PLAY_BIN (object);
  GstElement        *queue;
  GstElement        *capsfilter;
  gchar             *sink_factoryname;

  /* Create gstreamer elements */
  queue = _pt_make_element ("queue", "queue", NULL);
  capsfilter = _pt_make_element ("capsfilter", "audiofilter", NULL);

  /* Choose an audiosink ourselves instead of relying on autoaudiosink.
   * autoaudiosink is a fakesink until the first stream is loaded, so
   * we can't query the sink's properties until then. */

  if (have_pulseaudio_server ())
    sink_factoryname = "pulsesink";
  else
    sink_factoryname = "alsasink";

  self->audiosink = gst_element_factory_make (sink_factoryname, "audiosink");
  if (self->audiosink == NULL)
    {
      sink_factoryname = "autoaudiosink";
      self->audiosink = _pt_make_element (sink_factoryname, "audiosink", NULL);
    }

  g_object_set (self->audiosink, "enable-last-sample", TRUE, NULL);

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                    "MESSAGE", "Audio sink is %s", sink_factoryname);

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Audio sink implements stream volume: %s",
                    GST_IS_STREAM_VOLUME (self->audiosink) ? "yes" : "no");

  gst_bin_add_many (GST_BIN (self),
                    queue, capsfilter, self->audiosink, NULL);

  if (!gst_element_link_many (queue, capsfilter, self->audiosink, NULL))
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                        "MESSAGE", "ptaudioplaybin: elements not linked");
    }

  /* create ghost pad for audiosink */
  GstPad *audiopad = gst_element_get_static_pad (queue, "sink");
  gst_element_add_pad (GST_ELEMENT (self),
                       gst_ghost_pad_new ("sink", audiopad));
  gst_object_unref (GST_OBJECT (audiopad));
}

static void
gst_pt_audio_play_bin_init (GstPtAudioPlayBin *self)
{
}

static void
gst_pt_audio_play_bin_class_init (GstPtAudioPlayBinClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = gst_pt_audio_play_bin_constructed;
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_pt_audio_play_bin_debug, "ptaudioplaybin", 0,
                           "Audio playback bin for Parlatype");

  return (gst_element_register (plugin, "ptaudioplaybin",
                                GST_RANK_NONE, GST_TYPE_PT_AUDIO_PLAY_BIN));
}

/**
 * gst_pt_audio_play_bin_register:
 *
 * Registers a plugin holding our single element to use privately in this
 * library.
 *
 * Returns: TRUE if successful, otherwise FALSE
 */
gboolean
gst_pt_audio_play_bin_register (void)
{
  return gst_plugin_register_static (
      GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "ptaudioplaybin",
      "Audio playback bin for Parlatype",
      plugin_init,
      PACKAGE_VERSION,
      "GPL",
      "libparlatype",
      "Parlatype",
      PACKAGE_URL);
}
