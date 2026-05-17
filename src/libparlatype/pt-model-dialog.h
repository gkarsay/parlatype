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

#include "pt-config-list.h"
#include <adwaita.h>

#define PT_TYPE_MODEL_DIALOG (pt_model_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PtModelDialog, pt_model_dialog, PT, MODEL_DIALOG, AdwWindow)

PtModelDialog   *pt_model_dialog_new               (PtConfigList      *config_list);
