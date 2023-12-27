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

#include <parlatype.h>

#define PT_TYPE_CONFIG_LIST (pt_config_list_get_type ())
G_DECLARE_FINAL_TYPE (PtConfigList, pt_config_list, PT, CONFIG_LIST, GObject)

GFile* pt_config_list_get_folder (PtConfigList *self);

void pt_config_list_refresh (PtConfigList *self);

void pt_config_list_sort (PtConfigList *self);

PtConfigList *pt_config_list_new (PtPlayer *player);
