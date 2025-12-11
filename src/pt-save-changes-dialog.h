/* Copyright 2021 Christian Hergert <chergert@redhat.com>
 * Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Taken from gnome-text-editor/src/editor-save-changes-dialog-private.h */

#pragma once

#include <gtk/gtk.h>
#include <parlatype.h>

G_BEGIN_DECLS

void     pt_save_changes_dialog_run_async  (GtkWindow            *parent,
                                            PtTextView           *text_view,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);

gboolean pt_save_changes_dialog_run_finish (GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS
