/* Copyright 2026 Gabor Karsay <gabor.karsay@gmx.at>
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

#define PT_TYPE_CONFIG_ITEM (pt_config_item_get_type ())
G_DECLARE_FINAL_TYPE (PtConfigItem, pt_config_item, PT, CONFIG_ITEM, AdwSidebarItem)

void         pt_config_item_set_active    (PtConfigItem *self,
                                           gboolean     active);

gboolean     pt_config_item_get_active    (PtConfigItem *self);

gboolean     pt_config_item_is_installed  (PtConfigItem *self);

PtConfig    *pt_config_item_get_config    (PtConfigItem *self);

void         pt_config_item_set_config    (PtConfigItem *self,
                                           PtConfig    *config);

PtConfigItem *pt_config_item_new          (PtConfig    *config);
