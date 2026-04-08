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

#include <gio/gio.h>
#include "pt-window.h"

G_BEGIN_DECLS

#define PT_TYPE_WINDOW_ADDIN (pt_window_addin_get_type ())
G_DECLARE_INTERFACE (PtWindowAddin, pt_window_addin, PT, WINDOW_ADDIN, GObject)

struct _PtWindowAddinInterface
{
  GTypeInterface parent_interface;

  void (*load)   (PtWindowAddin *self,
                  PtWindow      *window);
  void (*unload) (PtWindowAddin *self);

};

void pt_window_addin_load   (PtWindowAddin *self,
                             PtWindow      *window);

void pt_window_addin_unload (PtWindowAddin *self);

G_END_DECLS
