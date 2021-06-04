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

Note 1: It doesn’t work if play_audio_play_bin or pt_audio_asr_bin are added to
        pt_audio_bin but are not linked! Either link it or remove it from bin!

*/

#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/audio/streamvolume.h>
#include "pt-config.h"
#include "gst-helpers.h"
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
	PROP_MUTE,
	PROP_VOLUME,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };


static GstPadProbeReturn
remove_element_cb (GstPad          *pad,
                   GstPadProbeInfo *info,
                   gpointer         user_data)
{
	GstElement *child = GST_ELEMENT (user_data);
	GstPad     *sinkpad;
	GstObject  *parent;

	gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

	parent = gst_element_get_parent (child);
	if (!parent) {
		GST_DEBUG_OBJECT (child, "%s has no parent", GST_OBJECT_NAME (child));
		return GST_PAD_PROBE_OK;
	}

	/* Unlink upstream, i.e. child's sink pad */
	sinkpad = gst_element_get_static_pad (child, "sink");
	GST_DEBUG_OBJECT (child, "unlinking %s", GST_OBJECT_NAME (child));
	gst_pad_unlink (pad, sinkpad);

	/* TODO investigate if flushing necessary */

	/* set to null or ready */
	gst_element_set_state (child, GST_STATE_READY);

	/* remove from bin, dereferences removed element, we want to keep it */
	GST_DEBUG_OBJECT (child, "removing %s from %s", GST_OBJECT_NAME (child), GST_OBJECT_NAME (parent));
	gst_object_ref (child);
	gst_bin_remove (GST_BIN (parent), child);

	gst_object_unref (parent);
	g_object_unref (sinkpad);

	return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
add_element_cb (GstPad          *pad,
                GstPadProbeInfo *info,
                gpointer         user_data)
{
	GstElement *child = GST_ELEMENT (user_data);
	GstPad     *sinkpad;
	GstObject  *queue;
	GstObject  *bin;
	GstObject  *parent;
	GstPadLinkReturn  r;

	gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

	/* check if in bin */
	queue = gst_pad_get_parent (pad);
	bin = gst_object_get_parent (queue);
	gst_object_unref (queue);

	parent = gst_element_get_parent (child);
	if (parent) {
		GST_DEBUG_OBJECT (child, "%s has already a parent %s",
		                  GST_OBJECT_NAME (child), GST_OBJECT_NAME (parent));
		gst_object_unref (parent);
		gst_object_unref (bin);
		return GST_PAD_PROBE_OK;
	}

	GST_DEBUG_OBJECT (child, "adding %s to %s",
	                  GST_OBJECT_NAME (child), GST_OBJECT_NAME (bin));
	gst_bin_add (GST_BIN (bin), child);
	gst_element_sync_state_with_parent (child);

	sinkpad = gst_element_get_static_pad (child, "sink");
	r = gst_pad_link (pad, sinkpad);
	g_assert (r == GST_PAD_LINK_OK);

	gst_object_unref (sinkpad);
	gst_object_unref (bin);

	return GST_PAD_PROBE_OK;
}

void
gst_pt_audio_bin_setup_asr (GstPtAudioBin  *bin,
                            gboolean        state)
{
	if (state)
		gst_pad_add_probe (bin->queue_src_asr,
		                   GST_PAD_PROBE_TYPE_BLOCKING | GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
		                   add_element_cb, bin->asr_bin, NULL);
	else
		gst_pad_add_probe (bin->queue_src_asr,
		                   GST_PAD_PROBE_TYPE_BLOCKING | GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
		                   remove_element_cb, bin->asr_bin, NULL);
}

gboolean
gst_pt_audio_bin_configure_asr (GstPtAudioBin *self,
                                PtConfig      *config,
                                GError       **error)
{
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	gboolean result;
	GstPtAudioAsrBin *bin;

	bin = GST_PT_AUDIO_ASR_BIN (self->asr_bin);
	result = gst_pt_audio_asr_bin_configure_asr (bin, config, error);

	return result;
}

void
gst_pt_audio_bin_setup_player (GstPtAudioBin  *bin,
                               gboolean        state)
{
	if (state)
		gst_pad_add_probe (bin->queue_src_play,
		                   GST_PAD_PROBE_TYPE_BLOCKING | GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
		                   add_element_cb, bin->play_bin, NULL);
	else
		gst_pad_add_probe (bin->queue_src_play,
		                   GST_PAD_PROBE_TYPE_BLOCKING | GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
		                   remove_element_cb, bin->play_bin, NULL);
}

static void
unref_element_without_parent (GstElement *element)
{
	GstObject *parent;

	parent = gst_element_get_parent (element);
	if (!parent) {
		gst_element_set_state (element, GST_STATE_NULL);
		gst_object_unref (element);
	} else {
		gst_object_unref (parent);
	}
	element = NULL;
}

static void
gst_pt_audio_bin_dispose (GObject *object)
{
	GstPtAudioBin *bin = GST_PT_AUDIO_BIN (object);

	if (bin->play_bin)
		unref_element_without_parent (bin->play_bin);
	if (bin->asr_bin)
		unref_element_without_parent (bin->asr_bin);
	if (bin->queue_src_play)
		gst_object_unref (GST_OBJECT (bin->queue_src_play));
	if (bin->queue_src_asr)
		gst_object_unref (GST_OBJECT (bin->queue_src_asr));

	G_OBJECT_CLASS (gst_pt_audio_bin_parent_class)->dispose (object);
}

static void
gst_pt_audio_bin_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
	GstPtAudioBin *bin = GST_PT_AUDIO_BIN (object);

	switch (property_id) {
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
	GstElement *tee;
	GstPad     *tee_sink;
	GstPad     *tee_src1;
	GstPad     *tee_src2;
	GstPad     *queue_sink1;
	GstPad     *queue_sink2;

	gst_pt_audio_play_bin_register ();
	gst_pt_audio_asr_bin_register ();

	tee           = _pt_make_element ("tee",            "tee",           NULL);
	bin->queue    = _pt_make_element ("multiqueue",     "multiqueue",    NULL);
	bin->play_bin = _pt_make_element ("ptaudioplaybin", "play-audiobin", NULL);
	bin->asr_bin  = _pt_make_element ("ptaudioasrbin",  "asr-audiobin",  NULL);

	gst_bin_add_many (GST_BIN (bin), tee, bin->queue, NULL);

	tee_sink = gst_element_get_static_pad (tee, "sink");
	tee_src1 = gst_element_get_request_pad (tee, "src_%u");
	tee_src2 = gst_element_get_request_pad (tee, "src_%u");
	queue_sink1 = gst_element_get_request_pad (bin->queue, "sink_1");
	queue_sink2 = gst_element_get_request_pad (bin->queue, "sink_2");
	bin->queue_src_play = gst_element_get_static_pad (bin->queue, "src_1");
	bin->queue_src_asr  = gst_element_get_static_pad (bin->queue, "src_2");

	/* link tee with multiqueue */
	gst_pad_link (tee_src1, queue_sink1);
	gst_pad_link (tee_src2, queue_sink2);

	/* create ghost pad for audiosink */
	gst_element_add_pad (GST_ELEMENT (bin),
	                     gst_ghost_pad_new ("sink", tee_sink));

	gst_object_unref (GST_OBJECT (tee_sink));
	gst_object_unref (GST_OBJECT (tee_src1));
	gst_object_unref (GST_OBJECT (tee_src2));
	gst_object_unref (GST_OBJECT (queue_sink1));
	gst_object_unref (GST_OBJECT (queue_sink2));
}

static void
gst_pt_audio_bin_class_init (GstPtAudioBinClass *klass)
{
	G_OBJECT_CLASS (klass)->set_property = gst_pt_audio_bin_set_property;
	G_OBJECT_CLASS (klass)->get_property = gst_pt_audio_bin_get_property;
	G_OBJECT_CLASS (klass)->dispose      = gst_pt_audio_bin_dispose;

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
