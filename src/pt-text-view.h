/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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
#include <parlatype.h>

#define PT_TYPE_TEXT_VIEW (pt_text_view_get_type ())
G_DECLARE_FINAL_TYPE (PtTextView, pt_text_view, PT, TEXT_VIEW, GtkTextView)


gboolean       pt_text_view_get_timestamps_visible (PtTextView        *self);

void           pt_text_view_set_timestamps_visible (PtTextView        *self,
                                                    gboolean           visible);

const GFile   *pt_text_view_get_text_file          (PtTextView         *self);

void           pt_text_view_set_text_file          (PtTextView         *self,
                                                    GFile              *file);

const GFile   *pt_text_view_get_audio_file         (PtTextView         *self);

void           pt_text_view_set_audio_file         (PtTextView         *self,
                                                    GFile              *file);

void           pt_text_view_insert_timestamp       (PtTextView         *self);

void           pt_text_view_connect                (PtTextView         *self);

gboolean       pt_text_view_save_finish            (PtTextView         *self,
                                                    GAsyncResult       *result,
                                                    GError            **error);

void           pt_text_view_save_async             (PtTextView         *self,
                                                    GFile              *file,
                                                    GCancellable       *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer            user_data);
/*
gboolean       pt_text_view_load_finish            (PtTextView         *self,
                                                    GAsyncResult       *result,
                                                    GError            **error);

void           pt_text_view_load_async             (PtTextView         *self,
                                                    GFile              *file,
                                                    GCancellable       *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer            user_data);
*/
gboolean       pt_text_view_load                   (PtTextView         *self,
                                                    GFile              *file,
                                                    GError            **error);

PtTextView    *pt_text_view_new              (void);
