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

#include "config.h"
#include "pt-controller.h"
#include "pt-window.h"

#define PT_DBUS_SERVICE_TYPE (pt_dbus_service_get_type ())
#define PT_DBUS_SERVICE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_DBUS_SERVICE_TYPE, PtDbusService))
#define PT_IS_DBUS_SERVICE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_DBUS_SERVICE_TYPE))

typedef struct _PtDbusService PtDbusService;
typedef struct _PtDbusServiceClass PtDbusServiceClass;
typedef struct _PtDbusServicePrivate PtDbusServicePrivate;

struct _PtDbusService
{
  PtController parent;

  /*< private > */
  PtDbusServicePrivate *priv;
};

struct _PtDbusServiceClass
{
  PtControllerClass parent_class;
};

GType pt_dbus_service_get_type (void) G_GNUC_CONST;

void pt_dbus_service_start (PtDbusService *self);

PtDbusService *pt_dbus_service_new (PtWindow *win);
