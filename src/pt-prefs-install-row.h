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

#ifndef PT_PREFS_INSTALL_ROW_H
#define PT_PREFS_INSTALL_ROW_H

#include <adwaita.h>
#include <parlatype.h>

#define PT_TYPE_PREFS_INSTALL_ROW (pt_prefs_install_row_get_type ())
G_DECLARE_FINAL_TYPE (PtPrefsInstallRow, pt_prefs_install_row, PT, PREFS_INSTALL_ROW, AdwActionRow)

gboolean
pt_prefs_install_row_get_installed (PtPrefsInstallRow *self);

GtkWidget*
pt_prefs_install_row_new (PtConfig *config);

#endif