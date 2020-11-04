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
 * SECTION: gstptaudiobin
 * @short_description: Audio bin (GstBin) for Parlatype.
 *
 * An audio bin that can be connected to a playbin element via playbin's
 * audio-filter property. It supports normal playback and silent ASR output
 * using a parlasphinx element.
 */

/*
 .-------------------------------------------------------------------------.
 | pt_audio_bin                                                            |
 | .------.     .--------------------------------------------------------. |
 | | tee  |     | play_bin                                               | |
 | |      |     | .--------.      .-------------.      .---------------. | |
 | |      |     | | queue  |      | capsfilter  |      | autoaudiosink | | |
 sink    src-> sink       src -> sink          src -> sink             | | |
 | |      |     | '--------'      '-------------'      '---------------' | |
 | |      |     '--------------------------------------------------------' |
 | |      |                                                                |
 | |      |     .--------------------------------------------------------. |
 | |      |     | sphinx_bin                                             | |
 | |      |     | .--------.                                .----------. | |
 | |      |     | | queue  |         (audioconvert          | fakesink | | |
 | |     src-> sink       src -> ...  audioresample ... -> sink        | | |
 | |      |     | '--------'          pocketsphinx)         '----------' | |
 | '------'     '--------------------------------------------------------' |
 |                                                                         |
 '-------------------------------------------------------------------------'

Note 1: It doesn’t work if play_bin or sphinx_bin are added to audio_bin but are
        not linked! Either link it or remove it from bin!

Note 2: The original intent was to dynamically switch the tee element to either
        playback or ASR. Rethink this design, maybe tee element can be completely
        omitted. On the other hand, maybe they could be somehow synced to have
        audio and recognition in real time.

Note 3: autoaudiosink (in the upper right corner) was replaced by a chosen sink
        and an optional "volume" element.
*/

#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/audio/streamvolume.h>
#include "gstptaudiobin.h"


GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_bin_debug

#define parent_class gst_pt_audio_bin_parent_class

G_DEFINE_TYPE (GstPtAudioBin, gst_pt_audio_bin, GST_TYPE_BIN);


enum
{
	PROP_0,
	PROP_PLAYER,
	PROP_ASR,
	PROP_VOLUME,
	PROP_SPHINX,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };



static GstElement*
make_element (gchar   *factoryname,
              gchar   *name,
              GError **error)
{
	GstElement *result;

	result = gst_element_factory_make (factoryname, name);
	if (!result)
		g_set_error (error, GST_CORE_ERROR,
		             GST_CORE_ERROR_MISSING_PLUGIN,
		             _("Failed to load plugin “%s”."), factoryname);

	return result;
}

#define PROPAGATE_ERROR_NULL \
if (earlier_error != NULL) {\
	g_propagate_error (error, earlier_error);\
	return NULL;\
}

#define PROPAGATE_ERROR_FALSE \
if (earlier_error != NULL) {\
	g_propagate_error (error, earlier_error);\
	return FALSE;\
}

#ifdef HAVE_ASR
static GstElement*
create_sphinx_bin (GstPtAudioBin *bin,
                   GError   **error)
{
	GError *earlier_error = NULL;

	/* Create gstreamer elements */
	GstElement *queue;
	GstElement *audioconvert;
	GstElement *audioresample;
	GstElement *fakesink;

	bin->sphinx = G_OBJECT (make_element ("parlasphinx", "sphinx", &earlier_error));
	/* defined error propagation; skipping any cleanup */
	PROPAGATE_ERROR_NULL
	queue = make_element ("queue", "sphinx_queue", &earlier_error);
	PROPAGATE_ERROR_NULL
	audioconvert = make_element ("audioconvert", "audioconvert", &earlier_error);
	PROPAGATE_ERROR_NULL
	audioresample = make_element ("audioresample", "audioresample", &earlier_error);
	PROPAGATE_ERROR_NULL
	fakesink = make_element ("fakesink", "fakesink", &earlier_error);
	PROPAGATE_ERROR_NULL

	/* create audio output */
	GstElement *audio = gst_bin_new ("sphinx-audiobin");
	gst_bin_add_many (GST_BIN (audio), queue, audioconvert, audioresample, GST_ELEMENT (bin->sphinx), fakesink, NULL);
	gst_element_link_many (queue, audioconvert, audioresample, GST_ELEMENT (bin->sphinx), fakesink, NULL);

	/* create ghost pad for audiosink */
	GstPad *audiopad = gst_element_get_static_pad (queue, "sink");
	gst_element_add_pad (audio, gst_ghost_pad_new ("sink", audiopad));
	gst_object_unref (GST_OBJECT (audiopad));

	return audio;
}
#endif

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

static GstElement*
create_play_bin (GstPtAudioBin *bin,
                 GError   **error)
{
	GError *earlier_error = NULL;

	/* Create gstreamer elements */
	GstElement   *capsfilter;
	GstElement   *audiosink;
	GstElement   *queue;
	gchar        *sink;
	GObjectClass *sinkclass;
	GParamSpec   *has_volume;
	GParamSpec   *has_mute;

	capsfilter = make_element ("capsfilter", "audiofilter", &earlier_error);
	/* defined error propagation; skipping any cleanup */
	PROPAGATE_ERROR_NULL

	queue = make_element ("queue", "player_queue", &earlier_error);
	PROPAGATE_ERROR_NULL

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
	audiosink = make_element (sink, "audiosink", &earlier_error);
	if (earlier_error != NULL) {
		g_clear_error (&earlier_error);
		sink = "autoaudiosink";
		audiosink = make_element (sink, "audiosink", &earlier_error);
	}
	PROPAGATE_ERROR_NULL

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		          "MESSAGE", "Audio sink is %s", sink);

	/* Audiosinks without a "volume" property can be controlled by the
	 * playbin element, but there is a noticeable delay in setting the volume.
	 * Create a "volume" element for those audiosinks.
	 * Note: Query is generic, it's actually only for alsasink.
	 * The exception for directsoundsink is because "mute" doesn't work. */

	sinkclass = G_OBJECT_GET_CLASS(audiosink);
	has_volume = g_object_class_find_property (sinkclass, "volume");
	has_mute =   g_object_class_find_property (sinkclass, "mute");

	if ((!has_volume && !has_mute) || g_strcmp0 (sink, "directsoundsink") == 0) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		                  "MESSAGE", "Creating a \"volume\" element for %s", sink);
		bin->volume_changer = make_element ("volume", "volume", &earlier_error);
		PROPAGATE_ERROR_NULL
	}

	GstElement *audio = gst_bin_new ("player-audiobin");
	if (bin->volume_changer) {
		gst_bin_add_many (GST_BIN (audio), queue, capsfilter,
				  bin->volume_changer, audiosink, NULL);
		gst_element_link_many (queue, capsfilter,
				       bin->volume_changer, audiosink, NULL);
	} else {
		gst_bin_add_many (GST_BIN (audio), queue, capsfilter, audiosink, NULL);
		gst_element_link_many (queue, capsfilter, audiosink, NULL);
		bin->volume_changer = audiosink;
	}

	/* create ghost pad for audiosink */
	GstPad *audiopad = gst_element_get_static_pad (queue, "sink");
	gst_element_add_pad (audio, gst_ghost_pad_new ("sink", audiopad));
	gst_object_unref (GST_OBJECT (audiopad));

	return audio;
}

static void
link_tee (GstPad     *tee_srcpad,
          GstElement *sink_bin)
{
	GstPad           *sinkpad;
	GstPadLinkReturn  r;

	sinkpad = gst_element_get_static_pad (sink_bin, "sink");
	g_assert_nonnull (sinkpad);
	r = gst_pad_link (tee_srcpad, sinkpad);
	g_assert (r == GST_PAD_LINK_OK);
	gst_object_unref (sinkpad);
}

static gboolean
pt_player_setup_pipeline (GstPtAudioBin *bin,
                          GError   **error)
{
	GError *earlier_error = NULL;

	bin->play_bin = create_play_bin (bin, &earlier_error);
	PROPAGATE_ERROR_FALSE
#ifdef HAVE_ASR
	bin->sphinx_bin = create_sphinx_bin (bin, &earlier_error);
	PROPAGATE_ERROR_FALSE
#endif
	bin->tee = make_element ("tee", "tee", &earlier_error);
	PROPAGATE_ERROR_FALSE
	bin->tee_playpad = gst_element_get_request_pad (bin->tee, "src_%u");
	bin->tee_sphinxpad = gst_element_get_request_pad (bin->tee, "src_%u");

	gst_bin_add (GST_BIN (bin), bin->tee);

	/* create ghost pad for audiosink */
	GstPad *audiopad = gst_element_get_static_pad (bin->tee, "sink");
	gst_element_add_pad (GST_ELEMENT (bin), gst_ghost_pad_new ("sink", audiopad));
	gst_object_unref (GST_OBJECT (audiopad));

	return TRUE;
}

static void
remove_element (GstBin *parent,
                gchar  *child_name)
{
	GstElement *child;
	child = gst_bin_get_by_name (parent, child_name);
	if (!child)
		return;

	/* removing dereferences removed element, we want to keep it */
	gst_object_ref (child);
	gst_bin_remove (parent, child);
}

static void
add_element (GstBin     *parent,
             GstElement *child,
             GstPad     *srcpad)
{
	GstElement *cmp;
	gchar      *child_name;

	child_name = gst_element_get_name (child);
	cmp = gst_bin_get_by_name (parent, child_name);
	g_free (child_name);
	if (cmp) {
		gst_object_unref (cmp);
		/* element is already in bin */
		return;
	}

	gst_bin_add (parent, child);
	link_tee (srcpad, child);
}

gboolean
gst_pt_audio_bin_setup_sphinx (GstPtAudioBin  *bin,
                               GError        **error)
{
	GError *earlier_error = NULL;

	if (!bin->tee)
		pt_player_setup_pipeline (bin, &earlier_error);
	PROPAGATE_ERROR_FALSE

	remove_element (GST_BIN (bin), "player-audiobin");
	add_element (GST_BIN (bin),
			bin->sphinx_bin, bin->tee_sphinxpad);

	return TRUE;
}

gboolean
gst_pt_audio_bin_setup_player (GstPtAudioBin  *bin,
                               GError       **error)
{
	GError *earlier_error = NULL;

	if (!bin->tee)
		pt_player_setup_pipeline (bin, &earlier_error);
	PROPAGATE_ERROR_FALSE

	remove_element (GST_BIN (bin), "sphinx-audiobin");
	add_element (GST_BIN (bin),
			bin->play_bin, bin->tee_playpad);

	return TRUE;
}

static void
gst_pt_audio_bin_dispose (GObject *object)
{
	GstPtAudioBin *bin = GST_PT_AUDIO_BIN (object);

	if (bin->tee) {
#ifdef HAVE_ASR
		/* Add all possible elements because elements without a parent
		   won't be destroyed. */
		add_element (GST_BIN (bin),
		             bin->play_bin, bin->tee_playpad);
		add_element (GST_BIN (bin),
		             bin->sphinx_bin, bin->tee_sphinxpad);
#endif
		gst_object_unref (GST_OBJECT (bin->tee_playpad));
		gst_object_unref (GST_OBJECT (bin->tee_sphinxpad));
	}

	G_OBJECT_CLASS (gst_pt_audio_bin_parent_class)->dispose (object);
}

static void
gst_pt_audio_bin_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
	GstPtAudioBin *bin = GST_PT_AUDIO_BIN (object);
	gboolean boolval;


	switch (property_id) {
	case PROP_PLAYER:
		boolval = g_value_get_boolean (value);
		if (boolval)
			gst_pt_audio_bin_setup_player (bin, NULL);
		break;
	case PROP_ASR:
		boolval = g_value_get_boolean (value);
		if (boolval)
			gst_pt_audio_bin_setup_sphinx (bin, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gst_pt_audio_bin_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
	GstPtAudioBin *bin = GST_PT_AUDIO_BIN (object);

	switch (property_id) {
	case PROP_PLAYER:
		g_value_set_boolean (value, bin->player);
		break;
	case PROP_ASR:
		g_value_set_boolean (value, bin->asr);
		break;
	case PROP_VOLUME:
		g_value_set_object (value, bin->volume_changer);
		break;
	case PROP_SPHINX:
		g_value_set_object (value, bin->sphinx);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gst_pt_audio_bin_init (GstPtAudioBin *bin)
{
	bin->sphinx = NULL;
	bin->volume_changer = NULL;
	bin->play_bin = NULL;
	bin->sphinx_bin = NULL;
	bin->tee = NULL;

	pt_player_setup_pipeline (bin, NULL);
	gst_pt_audio_bin_setup_player (bin, NULL);
}

static void
gst_pt_audio_bin_class_init (GstPtAudioBinClass *klass)
{
	G_OBJECT_CLASS (klass)->set_property = gst_pt_audio_bin_set_property;
	G_OBJECT_CLASS (klass)->get_property = gst_pt_audio_bin_get_property;
	G_OBJECT_CLASS (klass)->dispose      = gst_pt_audio_bin_dispose;

	obj_properties[PROP_PLAYER] =
	g_param_spec_boolean (
			"player",
			"Player",
			"Audio to player",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	obj_properties[PROP_ASR] =
	g_param_spec_boolean (
			"asr",
			"ASR",
			"Audio to ASR",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	obj_properties[PROP_VOLUME] =
	g_param_spec_object (
			"volume",
			"Volume",
			"Volume Changer Element",
			GST_TYPE_ELEMENT,
			G_PARAM_READABLE);

	obj_properties[PROP_SPHINX] =
	g_param_spec_object (
			"sphinx",
			"Sphinx",
			"Sphinx Element",
			G_TYPE_OBJECT,
			G_PARAM_READABLE);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

static gboolean
plugin_init (GstPlugin *plugin)
{
	GST_DEBUG_CATEGORY_INIT (gst_pt_audio_bin_debug, "ptaudiobin", 0,
	                         "Audio bin for Parlatype");

	return (gst_element_register (plugin, "ptaudiobin",
	                              GST_RANK_NONE, GST_TYPE_PT_AUDIO_BIN));
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  ptaudiobin,
                  "Audio bin for Parlatype",
                  plugin_init, PACKAGE_VERSION,
                  "GPL",
                  "Parlatype", "https://www.parlatype.org/")
