/* Copyright (C) Gabor Karsay 2019 <gabor.karsay@gmx.at>
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

#define PT_CONTROLLER_TYPE (pt_controller_get_type ())
G_DECLARE_DERIVABLE_TYPE (PtController, pt_controller, PT, CONTROLLER, GObject)

struct _PtControllerClass
{
  GObjectClass parent_class;
};

PtWindow *pt_controller_get_window (PtController *self);

PtPlayer *pt_controller_get_player (PtController *self);

PtController *pt_controller_new (PtWindow *win);
