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
 *  .--------------------------------------------------------------------.
 *  | pt_audio_asr_bin                                                   |
 *  | .-------.   .---------.   .----------.   .--------.   .----------. |
 *  | | queue |   | audio-  |   | audio-   |   | ASR    |   | fakesink | |
 *  -->       ----> convert ----> resample ----> Plugin ---->          | |
 *  | '-------'   '---------'   '----------'   '--------'   '----------' |
 *  '--------------------------------------------------------------------'
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
gst_pt_audio_asr_bin_is_configured (GstPtAudioAsrBin  *self)
{
	return self->is_configured;
}

gboolean
gst_pt_audio_asr_bin_configure_asr (GstPtAudioAsrBin  *self,
                                    PtConfig         *config,
                                    GError          **error)
{
	GST_DEBUG_OBJECT (self, "configuring asr");
	gchar    *plugin;
	gboolean  success;

	plugin = pt_config_get_plugin (config);

	/* If current plugin name is unlike new plugin name (or NULL) create
	 * the new plugin first */
	if (g_strcmp0 (self->asr_plugin_name, plugin) != 0) {
		if (self->asr_plugin) {
			GST_DEBUG_OBJECT (self, "removing previous plugin");
			gst_bin_remove (GST_BIN (self), self->asr_plugin);
		}

		self->asr_plugin = _pt_make_element (plugin, plugin, error);
		g_free (self->asr_plugin_name);

		if (!self->asr_plugin) {
			self->asr_plugin_name = NULL;
			self->is_configured = FALSE;
			return FALSE;
		}
		self->asr_plugin_name = g_strdup (plugin);
		gst_bin_add (GST_BIN (self), self->asr_plugin);
		gst_element_link_many (self->audioresample, self->asr_plugin, self->fakesink, NULL);
	}

	success = pt_config_apply (config, G_OBJECT (self->asr_plugin), error);
	self->is_configured = success;

	return success;
}

static void
gst_pt_audio_asr_bin_finalize (GObject *object)
{
	GstPtAudioAsrBin *self = GST_PT_AUDIO_ASR_BIN (object);

	g_free (self->asr_plugin_name);

	G_OBJECT_CLASS (parent_class)->finalize(object);
}

static void
gst_pt_audio_asr_bin_init (GstPtAudioAsrBin *bin)
{
	GstElement *queue;
	GstElement *audioconvert;

	queue              = _pt_make_element ("queue",         "asr_queue",     NULL);
	audioconvert       = _pt_make_element ("audioconvert",  "audioconvert",  NULL);
	bin->audioresample = _pt_make_element ("audioresample", "audioresample", NULL);
	bin->fakesink      = _pt_make_element ("fakesink",      "fakesink",      NULL);

	/* create audio output */
	gst_bin_add_many (GST_BIN (bin), queue, audioconvert, bin->audioresample,
	                  bin->fakesink, NULL);
	gst_element_link_many (queue, audioconvert, bin->audioresample, NULL);

	/* create ghost pad for audiosink */
	GstPad *audiopad = gst_element_get_static_pad (queue, "sink");
	gst_element_add_pad (GST_ELEMENT (bin), gst_ghost_pad_new ("sink", audiopad));
	gst_object_unref (GST_OBJECT (audiopad));

	bin->asr_plugin = NULL;
	bin->asr_plugin_name = NULL;
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
	GST_DEBUG_CATEGORY_INIT (gst_pt_audio_asr_bin_debug, "ptaudioasrbin", 0,
	                         "Audio bin for Automatic Speech Recognition for Parlatype");

	return (gst_element_register (plugin, "ptaudioasrbin",
	                              GST_RANK_NONE, GST_TYPE_PT_AUDIO_ASR_BIN));
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
			"Audio bin for Automatic Speech Recognition for Parlatype",
			plugin_init,
			PACKAGE_VERSION,
			"GPL",
			"libparlatype",
			"Parlatype",
			"https://www.parlatype.org/");
}
