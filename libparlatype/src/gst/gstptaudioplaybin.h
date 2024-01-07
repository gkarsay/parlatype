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

#pragma once

#include <gst/gst.h>

#define GST_TYPE_PT_AUDIO_PLAY_BIN (gst_pt_audio_play_bin_get_type ())
G_DECLARE_FINAL_TYPE (GstPtAudioPlayBin, gst_pt_audio_play_bin, GST, PT_AUDIO_PLAY_BIN, GstBin)

gboolean gst_pt_audio_play_bin_register (void);
