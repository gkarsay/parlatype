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


#include "config.h"
#include "pt-window.h"
#include "pt-mpris.h"

struct _PtMprisPrivate
{
};

G_DEFINE_TYPE_WITH_PRIVATE (PtMpris, pt_mpris, PT_CONTROLLER_TYPE)


void
pt_mpris_start (PtMpris *self)
{
}

static void
pt_mpris_init (PtMpris *self)
{
	self->priv = pt_mpris_get_instance_private (self);
}

static void
pt_mpris_dispose (GObject *object)
{
	/*PtMpris *self = PT_MPRIS (object);*/
	G_OBJECT_CLASS (pt_mpris_parent_class)->dispose (object);
}

static void
pt_mpris_class_init (PtMprisClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = pt_mpris_dispose;
}

PtMpris *
pt_mpris_new (PtWindow *win)
{
	return g_object_new (PT_MPRIS_TYPE, "win", win, NULL);
}
