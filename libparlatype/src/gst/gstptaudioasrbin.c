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
#include "gstptaudioasrbin.h"


GST_DEBUG_CATEGORY_STATIC (gst_pt_audio_asr_bin_debug);
#define GST_CAT_DEFAULT gst_pt_audio_asr_bin_debug

#define parent_class gst_pt_audio_asr_bin_parent_class

G_DEFINE_TYPE (GstPtAudioAsrBin, gst_pt_audio_asr_bin, GST_TYPE_BIN);


static GstElement*
make_element (gchar   *factoryname,
              gchar   *name)
{
	GstElement *result;

	result = gst_element_factory_make (factoryname, name);
	if (!result)
		g_log_structured (
			G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "MESSAGE",
			_("Failed to load plugin “%s”."), factoryname);

	return result;
}

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
	gchar *plugin;

	plugin = pt_config_get_plugin (config);
	if (!self->asr_plugin_name ||
	    g_strcmp0 (self->asr_plugin_name, plugin) != 0) {
		if (self->asr_plugin)
			gst_object_unref (self->asr_plugin);
		g_free (self->asr_plugin_name);
		self->asr_plugin_name = NULL;
		self->asr_plugin = make_element (plugin, plugin);
		if (!self->asr_plugin) {
			g_set_error (error,
			             GST_CORE_ERROR,
			             GST_CORE_ERROR_MISSING_PLUGIN,
			             _("Failed to load plugin “%s”."), plugin);
			g_free (plugin);
			return FALSE;
		}
		self->asr_plugin_name = g_strdup (plugin);
		gst_bin_add (GST_BIN (self), self->asr_plugin);
		gst_element_link_many (self->audioresample, self->asr_plugin, self->fakesink, NULL);
	}

	pt_config_apply (config, G_OBJECT (self->asr_plugin));
	self->is_configured = TRUE;

	g_free (plugin);
	return TRUE;
}

static void
gst_pt_audio_asr_bin_init (GstPtAudioAsrBin *bin)
{
	GstElement *queue;
	GstElement *audioconvert;

	queue              = make_element ("queue",         "sphinx_queue");
	audioconvert       = make_element ("audioconvert",  "audioconvert");
	bin->audioresample = make_element ("audioresample", "audioresample");
	bin->fakesink      = make_element ("fakesink",      "fakesink");

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
