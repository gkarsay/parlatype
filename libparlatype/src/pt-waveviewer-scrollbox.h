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

#include <gtk/gtk.h>

#define PT_TYPE_WAVEVIEWER_SCROLLBOX (pt_waveviewer_scrollbox_get_type ())
G_DECLARE_FINAL_TYPE (PtWaveviewerScrollbox, pt_waveviewer_scrollbox, PT, WAVEVIEWER_SCROLLBOX, GtkBox)

void       pt_waveviewer_scrollbox_set (PtWaveviewerScrollbox *self,
                                        gint                   width);

GtkWidget *pt_waveviewer_scrollbox_new (void);
