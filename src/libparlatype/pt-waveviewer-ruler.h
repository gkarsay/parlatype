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

#define PT_TYPE_WAVEVIEWER_RULER (pt_waveviewer_ruler_get_type ())
G_DECLARE_FINAL_TYPE (PtWaveviewerRuler, pt_waveviewer_ruler, PT, WAVEVIEWER_RULER, GtkWidget)

void       pt_waveviewer_ruler_set_ruler (PtWaveviewerRuler *self,
                                          gint64             n_samples,
                                          gint               px_per_sec,
                                          gint64             duration);

GtkWidget *pt_waveviewer_ruler_new       (void);
