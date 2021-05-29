/* Copyright (C) Gabor Karsay 2016 <gabor.karsay@gmx.at>
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

#ifndef PT_WINDOW_H
#define PT_WINDOW_H

#include "config.h"
#include <gtk/gtk.h>
#include <parlatype.h>
#include "pt-app.h"

#define PT_WINDOW_TYPE		(pt_window_get_type())
#define PT_WINDOW(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_WINDOW_TYPE, PtWindow))
#define PT_IS_WINDOW(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_WINDOW_TYPE))

typedef struct _PtWindow	PtWindow;
typedef struct _PtWindowClass	PtWindowClass;
typedef struct _PtWindowPrivate PtWindowPrivate;

struct _PtWindow
{
	GtkApplicationWindow  parent;
	PtPlayer             *player;

	/*< private > */
	PtWindowPrivate *priv;
};

struct _PtWindowClass
{
	GtkApplicationWindowClass parent_class;
};

GType		pt_window_get_type	(void) G_GNUC_CONST;

void		pt_error_message	(PtWindow    *parent,
					 const gchar *message,
					 const gchar *secondary_message);

void		pt_window_open_file	(PtWindow *win,
					 gchar    *uri);

gchar		*pt_window_get_uri	(PtWindow *win);

PtWindow	*pt_window_new		(PtApp *app);

#endif
