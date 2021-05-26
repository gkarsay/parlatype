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


#ifndef GST_PT_AUDIO_BIN_H
#define GST_PT_AUDIO_BIN_H

#include <gst/gst.h>
#include "pt-config.h"

G_BEGIN_DECLS

#define GST_TYPE_PT_AUDIO_BIN		(gst_pt_audio_bin_get_type())
#define GST_PT_AUDIO_BIN(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PT_AUDIO_BIN, GstPtAudioBin))
#define GST_IS_PT_AUDIO_BIN(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PT_AUDIO_BIN))
#define GST_PT_AUDIO_BIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PT_AUDIO_BIN, GstPtAudioBinClass))
#define GST_IS_PT_AUDIO_BIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PT_AUDIO_BIN))


typedef struct _GstPtAudioBin		GstPtAudioBin;
typedef struct _GstPtAudioBinClass	GstPtAudioBinClass;

struct _GstPtAudioBin
{
	GstBin parent;

	GstElement *play_bin;
	GstElement *asr_bin;
	GstElement *tee;
	GstPad     *tee_playpad;
	GstPad     *tee_asrpad;

	/* properties */
	gboolean    player;
	gboolean    asr;
	gfloat      volume;
	gboolean    mute;
};

struct _GstPtAudioBinClass
{
	GstBinClass parent_class;
};


GType		gst_pt_audio_bin_get_type	(void) G_GNUC_CONST;
void		gst_pt_audio_bin_setup_asr	(GstPtAudioBin  *bin,
						 gboolean        state);
gboolean	gst_pt_audio_bin_configure_asr	(GstPtAudioBin  *bin,
						 PtConfig  *config,
						 GError   **error);
void		gst_pt_audio_bin_setup_player	(GstPtAudioBin  *bin,
						 gboolean        state);
gboolean	gst_pt_audio_bin_register	(void);


G_END_DECLS

#endif
