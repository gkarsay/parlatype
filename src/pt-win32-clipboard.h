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


#ifndef PT_WIN32CLIPBOARD_H
#define PT_WIN32CLIPBOARD_H

#include "config.h"
#include "pt-controller.h"
#include "pt-window.h"

#define PT_WIN32CLIPBOARD_TYPE		(pt_win32clipboard_get_type())
#define PT_WIN32CLIPBOARD(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_WIN32CLIPBOARD_TYPE, PtWin32clipboard))
#define PT_IS_WIN32CLIPBOARD(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_WIN32CLIPBOARD_TYPE))

typedef struct _PtWin32clipboard		PtWin32clipboard;
typedef struct _PtWin32clipboardClass	PtWin32clipboardClass;
typedef struct _PtWin32clipboardPrivate	PtWin32clipboardPrivate;

struct _PtWin32clipboard
{
	PtController parent;

	/*< private > */
	PtWin32clipboardPrivate *priv;
};

struct _PtWin32clipboardClass
{
	PtControllerClass parent_class;
};


GType			pt_win32clipboard_get_type	(void) G_GNUC_CONST;

void			pt_win32clipboard_start		(PtWin32clipboard *self);

PtWin32clipboard 	*pt_win32clipboard_new		(PtWindow *win);

#endif
