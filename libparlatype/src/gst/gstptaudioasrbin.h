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


#ifndef GST_PT_AUDIO_ASR_BIN_H
#define GST_PT_AUDIO_ASR_BIN_H

#include <gst/gst.h>
#include "pt-config.h"

G_BEGIN_DECLS

#define GST_TYPE_PT_AUDIO_ASR_BIN		(gst_pt_audio_asr_bin_get_type())
#define GST_PT_AUDIO_ASR_BIN(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PT_AUDIO_ASR_BIN, GstPtAudioAsrBin))
#define GST_IS_PT_AUDIO_ASR_BIN(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PT_AUDIO_ASR_BIN))
#define GST_PT_AUDIO_ASR_BIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PT_AUDIO_ASR_BIN, GstPtAudioAsrBinClass))
#define GST_IS_PT_AUDIO_ASR_BIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PT_AUDIO_ASR_BIN))


typedef struct _GstPtAudioAsrBin	GstPtAudioAsrBin;
typedef struct _GstPtAudioAsrBinClass	GstPtAudioAsrBinClass;

struct _GstPtAudioAsrBin
{
	GstBin parent;

	gchar      *asr_plugin_name;
	GstElement *asr_plugin;
	GstElement *audioresample;
	GstElement *fakesink;
	gboolean    is_configured;
};

struct _GstPtAudioAsrBinClass
{
	GstBinClass parent_class;
};


GType		gst_pt_audio_asr_bin_get_type	 (void) G_GNUC_CONST;

gboolean	gst_pt_audio_asr_bin_is_configured 	(GstPtAudioAsrBin  *self);
gboolean	gst_pt_audio_asr_bin_configure_asr	(GstPtAudioAsrBin  *self,
							 PtConfig         *config,
							 GError          **error);

gboolean	gst_pt_audio_asr_bin_register	 (void);


G_END_DECLS

#endif
