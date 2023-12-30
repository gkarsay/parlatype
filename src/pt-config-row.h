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

#pragma once

#include <adwaita.h>
#include <parlatype.h>

#define PT_TYPE_CONFIG_ROW (pt_config_row_get_type ())
G_DECLARE_FINAL_TYPE (PtConfigRow, pt_config_row, PT, CONFIG_ROW, AdwActionRow)

void         pt_config_row_set_active   (PtConfigRow *self,
                                         gboolean     active);

gboolean     pt_config_row_get_active   (PtConfigRow *self);

gboolean     pt_config_row_is_installed (PtConfigRow *self);

PtConfig    *pt_config_row_get_config   (PtConfigRow *self);

void         pt_config_row_set_config   (PtConfigRow *self,
                                         PtConfig    *config);

PtConfigRow *pt_config_row_new          (PtConfig    *config);
