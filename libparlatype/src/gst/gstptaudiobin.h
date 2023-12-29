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

#pragma once

#include "pt-config.h"
#include "pt-player.h"
#include <gst/gst.h>

#define GST_TYPE_PT_AUDIO_BIN (gst_pt_audio_bin_get_type ())
G_DECLARE_FINAL_TYPE (GstPtAudioBin, gst_pt_audio_bin, GST, PT_AUDIO_BIN, GstBin)

gboolean gst_pt_audio_bin_configure_asr (GstPtAudioBin *self,
                                         PtConfig *config,
                                         GError **error);
PtModeType gst_pt_audio_bin_get_mode (GstPtAudioBin *self);
void gst_pt_audio_bin_set_mode (GstPtAudioBin *self,
                                PtModeType mode);
gboolean gst_pt_audio_bin_register (void);
