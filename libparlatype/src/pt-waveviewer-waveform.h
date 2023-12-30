/* Copyright (C) Gabor Karsay 2017 <gabor.karsay@gmx.at>
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

#include <gtk/gtk.h>

#define PT_TYPE_WAVEVIEWER_WAVEFORM (pt_waveviewer_waveform_get_type ())
G_DECLARE_FINAL_TYPE (PtWaveviewerWaveform, pt_waveviewer_waveform, PT, WAVEVIEWER_WAVEFORM, GtkDrawingArea)

void       pt_waveviewer_waveform_set (PtWaveviewerWaveform *self,
                                       GArray               *peaks);

GtkWidget *pt_waveviewer_waveform_new (void);
