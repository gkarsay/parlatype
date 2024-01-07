/* Copyright 2016 Gabor Karsay <gabor.karsay@gmx.at>
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

#define PT_TYPE_GOTO_DIALOG (pt_goto_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PtGotoDialog, pt_goto_dialog, PT, GOTO_DIALOG, GtkDialog)

gint          pt_goto_dialog_get_pos (PtGotoDialog *self);

void          pt_goto_dialog_set_pos (PtGotoDialog *self,
                                      gint          seconds);

void          pt_goto_dialog_set_max (PtGotoDialog *self,
                                      gint          seconds);

PtGotoDialog *pt_goto_dialog_new     (GtkWindow    *win);
