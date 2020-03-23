/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
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


#include "config.h"
#include <gtk/gtk.h>
#include "pt-window.h"
#include "pt-win32clipboard.h"

struct _PtWin32clipboardPrivate
{
	GDBusProxy *proxy;
	gint        dbus_watch_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtWin32clipboard, pt_win32clipboard, PT_CONTROLLER_TYPE)


void
pt_win32clipboard_start (PtWin32clipboard *self)
{
}

static void
pt_win32clipboard_init (PtWin32clipboard *self)
{
	self->priv = pt_win32clipboard_get_instance_private (self);


	HMODULE user32;
	user32 = LoadLibraryA ("user32.dll");

	if (user32 == NULL)
		return;





	self->priv->proxy = NULL;
	self->priv->dbus_watch_id = 0;
}

static void
pt_win32clipboard_dispose (GObject *object)
{
	PtWin32clipboard *self = PT_WIN32CLIPBOARD (object);
	g_clear_object (&self->priv->proxy);
	G_OBJECT_CLASS (pt_win32clipboard_parent_class)->dispose (object);
}

static void
pt_win32clipboard_class_init (PtWin32clipboardClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = pt_win32clipboard_dispose;
}

PtWin32clipboard *
pt_win32clipboard_new (PtWindow *win)
{
	return g_object_new (PT_WIN32CLIPBOARD_TYPE, "win", win, NULL);
}
