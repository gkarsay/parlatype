/* Copyright 2017 Gabor Karsay <gabor.karsay@gmx.at>
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

#define PT_TYPE_WAVEVIEWER_CURSOR (pt_waveviewer_cursor_get_type ())
G_DECLARE_FINAL_TYPE (PtWaveviewerCursor, pt_waveviewer_cursor, PT, WAVEVIEWER_CURSOR, GtkDrawingArea)

void       pt_waveviewer_cursor_render (PtWaveviewerCursor *self,
                                        gint                position);

GtkWidget *pt_waveviewer_cursor_new    (void);
