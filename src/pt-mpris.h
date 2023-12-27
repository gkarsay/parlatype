/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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

#define PT_MPRIS_TYPE (pt_mpris_get_type ())
#define PT_MPRIS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_MPRIS_TYPE, PtMpris))
#define PT_IS_MPRIS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_MPRIS_TYPE))

typedef struct _PtMpris PtMpris;
typedef struct _PtMprisClass PtMprisClass;
typedef struct _PtMprisPrivate PtMprisPrivate;

struct _PtMpris
{
  PtController parent;

  /*< private > */
  PtMprisPrivate *priv;
};

struct _PtMprisClass
{
  PtControllerClass parent_class;
};

GType pt_mpris_get_type (void) G_GNUC_CONST;

void pt_mpris_start (PtMpris *self);

PtMpris *pt_mpris_new (PtWindow *win);
