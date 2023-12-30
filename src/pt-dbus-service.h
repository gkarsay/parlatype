/* Copyright (C) Gabor Karsay 2016, 2019 <gabor.karsay@gmx.at>
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

#include "pt-controller.h"

#define PT_DBUS_SERVICE_TYPE (pt_dbus_service_get_type ())
G_DECLARE_FINAL_TYPE (PtDbusService, pt_dbus_service, PT, DBUS_SERVICE, PtController)

void           pt_dbus_service_start (PtDbusService *self);

PtDbusService *pt_dbus_service_new   (PtWindow      *win);
