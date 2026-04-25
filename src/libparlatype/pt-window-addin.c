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

/**
 * PtWindowAddin:
 *
 * Interface for plugins which want to extend the main window.
 */

#define G_LOG_DOMAIN "pt-window-addin"

#include "config.h"

#include "pt-window-addin.h"

#include "pt-window.h"

G_DEFINE_INTERFACE (PtWindowAddin, pt_window_addin, G_TYPE_OBJECT)

static void
pt_window_addin_real_load (PtWindowAddin *self,
                           PtWindow      *window)
{
}

static void
pt_window_addin_real_unload (PtWindowAddin *self)
{
}

static void
pt_window_addin_default_init (PtWindowAddinInterface *iface)
{
  iface->load = pt_window_addin_real_load;
  iface->unload = pt_window_addin_real_unload;
}

/**
 * pt_window_addin_load:
 * @self: a #PtWindowAddin
 * @window: a handle on the window
 *
 * Plugins should start their work here.
 */
void
pt_window_addin_load (PtWindowAddin *self,
                      PtWindow      *window)
{
  PtWindowAddinInterface *iface;

  g_return_if_fail (PT_IS_WINDOW_ADDIN (self));

  iface = PT_WINDOW_ADDIN_GET_IFACE (self);
  g_return_if_fail (iface->load != NULL);

  iface->load (self, window);
}

/**
 * pt_window_addin_unload:
 * @self: an #PtWindowAddin
 *
 * Plugins should undo their work here.
 */
void
pt_window_addin_unload (PtWindowAddin *self)
{
  PtWindowAddinInterface *iface;

  g_return_if_fail (PT_IS_WINDOW_ADDIN (self));

  iface = PT_WINDOW_ADDIN_GET_IFACE (self);
  g_return_if_fail (iface->unload != NULL);

  iface->unload (self);
}
