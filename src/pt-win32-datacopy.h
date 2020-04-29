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


#ifndef PT_WIN32_DATACOPY_H
#define PT_WIN32_DATACOPY_H

#include "config.h"
#include "pt-controller.h"
#include "pt-window.h"

#define PT_WIN32_DATACOPY_TYPE		(pt_win32_datacopy_get_type())
#define PT_WIN32_DATACOPY(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_WIN32_DATACOPY_TYPE, PtWin32Datacopy))
#define PT_IS_WIN32_DATACOPY(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_WIN32_DATACOPY_TYPE))

#define PT_HIDDEN_WINDOW_CLASS "ParlatypeHiddenWindowClass"
#define PT_HIDDEN_WINDOW_TITLE "ParlatypeHiddenWindowTitle"

typedef struct _PtWin32Datacopy		PtWin32Datacopy;
typedef struct _PtWin32DatacopyClass	PtWin32DatacopyClass;
typedef struct _PtWin32DatacopyPrivate	PtWin32DatacopyPrivate;

enum
{
	/* Methods without response */

	PLAY_PAUSE,
	GOTO_TIMESTAMP,
	JUMP_BACK,
	JUMP_FORWARD,
	DECREASE_SPEED,
	INCREASE_SPEED,

	/* Used internally for unique instance management */

	OPEN_FILE,
	PRESENT_WINDOW,

	/* Methods with response */

	GET_TIMESTAMP,
	GET_URI,

	/* Responses */

	RESPONSE_TIMESTAMP,
	RESPONSE_URI
};

struct _PtWin32Datacopy
{
	PtController parent;

	/*< private > */
	PtWin32DatacopyPrivate *priv;
};

struct _PtWin32DatacopyClass
{
	PtControllerClass parent_class;
};


GType			pt_win32_datacopy_get_type	(void) G_GNUC_CONST;

void			pt_win32_datacopy_start		(PtWin32Datacopy *self);

PtWin32Datacopy 	*pt_win32_datacopy_new		(PtWindow *win);

#endif
