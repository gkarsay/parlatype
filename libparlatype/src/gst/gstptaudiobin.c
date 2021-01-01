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
 * using a parlasphinx element.
 * .----------------------------------.
 * | pt_audio_bin                     |
 * | .------.   .-------------------. |
 * | | tee  |   | pt_audio_play_bin | |
 * -->      ---->                   | |
 * | |      |   |                   | |
 * | |      |   '-------------------' |
 * | |      |   .-------------------. |
 * | |      |   | pt_audio_asr_bin  | |
 * | |      ---->                   | |
 * | |      |   |                   | |
 * | '------'   '-------------------' |
 * '----------------------------------'

Note 1: It doesn’t work if play_bin or sphinx_bin are added to audio_bin but are
        not linked! Either link it or remove it from bin!

Note 2: The original intent was to dynamically switch the tee element to either
        playback or ASR. Rethink this design, maybe tee element can be completely
        omitted. On the other hand, maybe they could be somehow synced to have
        audio and recognition in real time.
*/

#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/audio/streamvolume.h>
#include "pt-config.h"
#include "gstptaudioasrbin.h"
#include "gstptaudioplaybin.h"
#include "gstptaudiobin.h"


GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_bin_debug

#define parent_class gst_pt_audio_bin_parent_class

G_DEFINE_TYPE_WITH_CODE (GstPtAudioBin, gst_pt_audio_bin, GST_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GST_TYPE_STREAM_VOLUME, NULL));

enum
{
	PROP_0,
	PROP_PLAYER,
	PROP_ASR,
	PROP_MUTE,
	PROP_VOLUME,
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

	bin->play_bin = make_element ("ptaudioplaybin", "player-audiobin", &earlier_error);
	PROPAGATE_ERROR_FALSE

#ifdef HAVE_ASR
	bin->sphinx_bin = make_element("ptaudioasrbin", "sphinx-audiobin", &earlier_error);
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
	//GError *earlier_error = NULL;

	remove_element (GST_BIN (bin), "player-audiobin");
	add_element (GST_BIN (bin),
			bin->sphinx_bin, bin->tee_sphinxpad);

	return TRUE;
}

gboolean
gst_pt_audio_bin_configure_asr (GstPtAudioBin  *self,
                                PtConfig      *config,
                                GError       **error)
{
	gboolean result;
	GstPtAudioAsrBin *bin;

	bin = GST_PT_AUDIO_ASR_BIN (self->sphinx_bin);
	result = gst_pt_audio_asr_bin_configure_asr (bin, config, error);

	return result;
}

gboolean
gst_pt_audio_bin_setup_player (GstPtAudioBin  *bin,
                               GError       **error)
{
	//GError *earlier_error = NULL;

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
		//add_element (GST_BIN (bin),
		//             bin->sphinx_bin, bin->tee_sphinxpad);
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
	case PROP_MUTE:
		g_object_set (bin->play_bin,
		              "mute", g_value_get_boolean (value), NULL);
		break;
	case PROP_VOLUME:
		g_object_set (bin->play_bin,
		              "volume", g_value_get_double (value), NULL);
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
	gboolean mute;
	gdouble  volume;

	switch (property_id) {
	case PROP_PLAYER:
		g_value_set_boolean (value, bin->player);
		break;
	case PROP_ASR:
		g_value_set_boolean (value, bin->asr);
		break;
	case PROP_MUTE:
		g_object_get (bin->play_bin, "mute", &mute, NULL);
		g_value_set_boolean (value, mute);
		break;
	case PROP_VOLUME:
		g_object_get (bin->play_bin, "volume", &volume, NULL);
		g_value_set_double (value, volume);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gst_pt_audio_bin_init (GstPtAudioBin *bin)
{
	bin->play_bin = NULL;
	bin->sphinx_bin = NULL;
	bin->tee = NULL;

	gst_pt_audio_play_bin_register ();
	gst_pt_audio_asr_bin_register ();
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
