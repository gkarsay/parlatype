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

#include "pt-window.h"

#include <parlatype.h>

PtPlayer         *_pt_window_get_player              (PtWindow *self);

GtkWidget        *_pt_window_get_waveviewer          (PtWindow *self);

GtkRecentManager *_pt_window_get_recent_manager      (PtWindow *self);

GtkWidget        *_pt_window_get_primary_menu_button (PtWindow *self);

GSettings        *_pt_window_get_settings            (PtWindow *self);
