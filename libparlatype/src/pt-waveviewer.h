/* Copyright (C) Gabor Karsay 2016 <gabor.karsay@gmx.at>
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

#if !defined(__PARLATYPE_H_INSIDE__) && !defined(PARLATYPE_COMPILATION)
#error "Only <parlatype.h> can be included directly."
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVEVIEWER (pt_waveviewer_get_type ())
G_DECLARE_DERIVABLE_TYPE (PtWaveviewer, pt_waveviewer, PT, WAVEVIEWER, GtkWidget)


struct _PtWaveviewerClass
{
  GtkWidgetClass klass;
};

gboolean pt_waveviewer_get_follow_cursor (PtWaveviewer *self);

void pt_waveviewer_set_follow_cursor (PtWaveviewer *self,
                                      gboolean follow);

gboolean pt_waveviewer_load_wave_finish (PtWaveviewer *self,
                                         GAsyncResult *result,
                                         GError **error);

void pt_waveviewer_load_wave_async (PtWaveviewer *self,
                                    gchar *uri,
                                    GCancellable *cancel,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);

GtkWidget *pt_waveviewer_new (void);

G_END_DECLS
