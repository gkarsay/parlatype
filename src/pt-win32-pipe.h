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

#ifndef PT_WIN32_PIPE_H
#define PT_WIN32_PIPE_H

#include "config.h"
#include "pt-controller.h"
#include "pt-window.h"

#define PT_WIN32_PIPE_TYPE (pt_win32_pipe_get_type ())
#define PT_WIN32_PIPE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_WIN32_PIPE_TYPE, PtWin32Pipe))
#define PT_IS_WIN32_PIPE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_WIN32_PIPE_TYPE))

typedef struct _PtWin32Pipe PtWin32Pipe;
typedef struct _PtWin32PipeClass PtWin32PipeClass;
typedef struct _PtWin32PipePrivate PtWin32PipePrivate;

struct _PtWin32Pipe
{
  PtController parent;

  /*< private > */
  PtWin32PipePrivate *priv;
};

struct _PtWin32PipeClass
{
  PtControllerClass parent_class;
};

GType pt_win32_pipe_get_type (void) G_GNUC_CONST;

void pt_win32_pipe_start (PtWin32Pipe *self);

void pt_win32_pipe_stop (PtWin32Pipe *self);

PtWin32Pipe *pt_win32_pipe_new (PtWindow *win);

#endif
