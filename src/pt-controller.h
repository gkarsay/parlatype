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

#include "config.h"
#include "pt-window.h"

#define PT_CONTROLLER_TYPE (pt_controller_get_type ())
#define PT_CONTROLLER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_CONTROLLER_TYPE, PtController))
#define PT_IS_CONTROLLER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_CONTROLLER_TYPE))

typedef struct _PtController PtController;
typedef struct _PtControllerClass PtControllerClass;
typedef struct _PtControllerPrivate PtControllerPrivate;

struct _PtController
{
  GObject parent;

  /*< private > */
  PtControllerPrivate *priv;
};

struct _PtControllerClass
{
  GObjectClass parent_class;
};

GType pt_controller_get_type (void) G_GNUC_CONST;

PtWindow *pt_controller_get_window (PtController *self);

PtPlayer *pt_controller_get_player (PtController *self);

PtController *pt_controller_new (PtWindow *win);
