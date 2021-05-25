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
 * gstptaudioplaybin
 * Child bin (GstBin) for parent PtAudioBin.
 *
 * This bin is an audio sink for normal playback.
 *
 *  .-----------------------------------------------------------------.
 *  | pt_audio_play_bin                                               |
 *  | .--------.   .-------------.   .-------------.   .------------. |
 *  | | queue  |   | capsfilter  |   | volume      |   | audiosink  | |
 *  -->        ---->             ----> (optional)  ---->            | |
 *  | '--------'   '-------------'   '-------------'   '------------' |
 *  '-----------------------------------------------------------------'
 *
 * PtAudioPlaybin implements GstStreamVolume, that means it has "volume" and
 * "mute" properties.
 *
 * audiosink is chosen at runtime. If the chosen audiosink does not implement
 * GstStreamVolume a volume element is added. PtAudioPlayBin relays audiosink's
 * "volume" and "mute" properties or those of the volume element.
 */

#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/audio/streamvolume.h>
#include "gst-helpers.h"
#include "gstptaudioplaybin.h"


GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_play_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_play_bin_debug

#define parent_class gst_pt_audio_play_bin_parent_class

G_DEFINE_TYPE_WITH_CODE (GstPtAudioPlayBin, gst_pt_audio_play_bin, GST_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GST_TYPE_STREAM_VOLUME, NULL));

enum
{
	PROP_0,
	PROP_VOLUME,
	PROP_MUTE,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };


#ifndef G_OS_WIN32
static gboolean
have_pulseaudio_server (void)
{
	/* Adapted from Quod Libet ...quodlibet/player/gstbe/util.py:
	 * If we have a pulsesink we can get the server presence through
	 * setting the ready state */
	GstElement           *pulse;
	GstStateChangeReturn  state;

	pulse = gst_element_factory_make ("pulsesink", NULL);
	if (!pulse)
		return FALSE;
	gst_element_set_state (pulse, GST_STATE_READY);
	state = gst_element_get_state (pulse, NULL, NULL, GST_CLOCK_TIME_NONE);
	gst_element_set_state (pulse, GST_STATE_NULL);
	gst_object_unref (pulse);
	return (state != GST_STATE_CHANGE_FAILURE);
}
#endif

static void
gst_pt_audio_play_bin_init (GstPtAudioPlayBin *bin)
{
	/* Create gstreamer elements */
	GstElement   *capsfilter;
	GstElement   *audiosink;
	GstElement   *queue;
	gchar        *sink;

	capsfilter = _pt_make_element ("capsfilter", "audiofilter",  NULL);
	queue      = _pt_make_element ("queue",      "player_queue", NULL);

	/* Choose an audiosink ourselves instead of relying on autoaudiosink.
	 * It chose waveformsink on win32 (not a good choice) and it will be
	 * a fakesink until the first stream is loaded, so we can't query the
	 * sinks properties until then. */

#ifdef G_OS_WIN32
	sink = "directsoundsink";
#else
	if (have_pulseaudio_server ())
		sink = "pulsesink";
	else
		sink = "alsasink";
#endif
	audiosink = gst_element_factory_make (sink, "audiosink");
	if (!audiosink) {
		sink = "autoaudiosink";
		audiosink = _pt_make_element (sink, "audiosink", NULL);
	}

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		          "MESSAGE", "Audio sink is %s", sink);

	/* Audiosinks without a "volume" property can be controlled by the
	 * playbin element, but there is a noticeable delay in setting the volume.
	 * Create a "volume" element for those audiosinks.
	 * The exception for directsoundsink is because "mute" doesn't work. */

	if (!GST_IS_STREAM_VOLUME (audiosink) ||
	    g_strcmp0 ("sink", "directsoundsink") == 0) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "MESSAGE",
		                  "Creating a \"volume\" element for %s", sink);
		bin->volume_changer = gst_element_factory_make ("volume", "volume");
	}

	if (bin->volume_changer) {
		gst_bin_add_many (GST_BIN (bin), queue, capsfilter,
				  bin->volume_changer, audiosink, NULL);
		gst_element_link_many (queue, capsfilter,
				       bin->volume_changer, audiosink, NULL);
	} else {
		gst_bin_add_many (GST_BIN (bin),
		                  queue, capsfilter, audiosink, NULL);
		gst_element_link_many (queue, capsfilter, audiosink, NULL);
		bin->volume_changer = audiosink;
	}

	/* create ghost pad for audiosink */
	GstPad *audiopad = gst_element_get_static_pad (queue, "sink");
	gst_element_add_pad (GST_ELEMENT (bin),
	                     gst_ghost_pad_new ("sink", audiopad));
	gst_object_unref (GST_OBJECT (audiopad));
}

static void
gst_pt_audio_play_bin_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	GstPtAudioPlayBin *bin = GST_PT_AUDIO_PLAY_BIN (object);

	switch (property_id) {
	case PROP_MUTE:
		g_object_set (bin->volume_changer,
		              "mute", g_value_get_boolean (value), NULL);
		break;
	case PROP_VOLUME:
		g_object_set (bin->volume_changer,
		              "volume", g_value_get_double (value), NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gst_pt_audio_play_bin_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GstPtAudioPlayBin *bin = GST_PT_AUDIO_PLAY_BIN (object);
	gboolean mute;
	gdouble  volume;

	switch (property_id) {
	case PROP_MUTE:
		g_object_get (bin->volume_changer, "mute", &mute, NULL);
		g_value_set_boolean (value, mute);
		break;
	case PROP_VOLUME:
		g_object_get (bin->volume_changer, "volume", &volume, NULL);
		g_value_set_double (value, volume);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gst_pt_audio_play_bin_class_init (GstPtAudioPlayBinClass *klass)
{
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = gst_pt_audio_play_bin_get_property;
	gobject_class->set_property = gst_pt_audio_play_bin_set_property;

	obj_properties[PROP_MUTE] =
	g_param_spec_boolean (
			"mute",
			"Mute",
			"mute channel",
			FALSE,
			G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
			                    G_PARAM_STATIC_STRINGS);

	obj_properties[PROP_VOLUME] =
	g_param_spec_double (
			"volume",
			"Volume",
			"volume factor, 1.0=100%",
			0.0, 1.0, 1.0,
			G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
			                    G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);

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
 * Return value: TRUE if successful, otherwise FALSE
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
			"https://www.parlatype.org/");
}
