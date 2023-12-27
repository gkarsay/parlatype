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

#include "config.h"
#include <adwaita.h>

#define PT_TYPE_PREFERENCES_DIALOG (pt_preferences_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PtPreferencesDialog, pt_preferences_dialog, PT, PREFERENCES_DIALOG, AdwPreferencesWindow)

void pt_show_preferences_dialog (GtkWindow *parent);

PtPreferencesDialog *pt_preferences_dialog_new (GtkWindow *parent);
