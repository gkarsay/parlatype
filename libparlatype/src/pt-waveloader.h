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

#include <gio/gio.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVELOADER (pt_waveloader_get_type ())
G_DECLARE_DERIVABLE_TYPE (PtWaveloader, pt_waveloader, PT, WAVELOADER, GObject)

struct _PtWaveloaderClass
{
  GObjectClass parent_class;
};

gboolean      pt_waveloader_load_finish   (PtWaveloader       *self,
                                           GAsyncResult       *result,
                                           GError            **error);

void          pt_waveloader_load_async    (PtWaveloader       *self,
                                           gint                pps,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data);

gint64        pt_waveloader_get_duration  (PtWaveloader       *self);

gboolean      pt_waveloader_resize_finish (PtWaveloader       *self,
                                           GAsyncResult       *result,
                                           GError            **error);

void          pt_waveloader_resize_async  (PtWaveloader       *self,
                                           gint                pps,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data);

gboolean      pt_waveloader_resize        (PtWaveloader       *self,
                                           gint                pps,
                                           GError            **error);

GArray       *pt_waveloader_get_data      (PtWaveloader       *self);

PtWaveloader *pt_waveloader_new           (gchar              *uri);

G_END_DECLS
