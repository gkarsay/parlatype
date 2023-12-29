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

#include <gtk/gtk.h>
#include "pt-app.h"

#define PT_WINDOW_TYPE (pt_window_get_type ())
G_DECLARE_FINAL_TYPE (PtWindow, pt_window, PT, WINDOW, GtkApplicationWindow)


struct _PtWindowClass
{
  GtkApplicationWindowClass parent_class;
};

GType pt_window_get_type (void) G_GNUC_CONST;

void pt_error_message (PtWindow *parent,
                       const gchar *message,
                       const gchar *secondary_message);

void pt_window_open_file (PtWindow *self,
                          gchar *uri);

gchar *pt_window_get_uri (PtWindow *self);

PtWindow *pt_window_new (PtApp *app);
