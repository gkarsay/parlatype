/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
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

#ifndef PT_WIN32_HELPERS_H
#define PT_WIN32_HELPERS_H

#include "config.h"

gchar *pt_win32_get_help_uri (void);
void pt_win32_add_audio_patterns (GtkFileFilter *filter);
gboolean pt_win32_present_other_instance (GApplication *app);
gboolean pt_win32_open_in_other_instance (GApplication *app,
                                          gchar *uri);
gchar *pt_win32_get_last_error_msg (void);

#endif
