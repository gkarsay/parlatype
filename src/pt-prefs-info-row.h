/* Copyright (C) Gabor Karsay 2023 <gabor.karsay@gmx.at>
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

#include <adwaita.h>

#define PT_TYPE_PREFS_INFO_ROW (pt_prefs_info_row_get_type ())
G_DECLARE_FINAL_TYPE (PtPrefsInfoRow, pt_prefs_info_row, PT, PREFS_INFO_ROW, AdwActionRow)

void            pt_prefs_info_row_set_title (PtPrefsInfoRow *self,
                                             gchar          *title);

void            pt_prefs_info_row_set_info  (PtPrefsInfoRow *self,
                                             gchar          *info);

PtPrefsInfoRow *pt_prefs_info_row_new       (gchar          *title,
                                             gchar          *info);
