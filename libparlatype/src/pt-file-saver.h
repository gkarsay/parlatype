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

#define PT_TYPE_FILE_SAVER (pt_file_saver_get_type ())
G_DECLARE_FINAL_TYPE (PtFileSaver, pt_file_saver, PT, FILE_SAVER, GObject)

gboolean        pt_file_saver_save_finish (PtFileSaver  *self,
                                           GAsyncResult *result,
                                           GError      **error);

void            pt_file_saver_save_async (PtFileSaver          *self,
                                          gint                  io_priority,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);

PtFileSaver    *pt_file_saver_new        (PtTextView *view,
                                          GFile      *file);
