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
#include "pt-text-view.h"

#define PT_TYPE_FILE_LOADER (pt_file_loader_get_type ())
G_DECLARE_FINAL_TYPE (PtFileLoader, pt_file_loader, PT, FILE_LOADER, GObject)

/*
gboolean        pt_file_loader_load_finish (PtFileLoader         *self,
                                            GAsyncResult        *result,
                                            GError             **error);

void            pt_file_loader_load_async  (PtFileLoader        *self,
                                            gint                 io_priority,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);
*/
gboolean        pt_file_loader_load        (PtFileLoader         *self,
                                            GError              **error);

PtFileLoader   *pt_file_loader_new         (PtTextView           *view,
                                            GFile                *file);
