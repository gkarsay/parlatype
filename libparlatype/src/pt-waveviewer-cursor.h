/* Copyright (C) Gabor Karsay 2017 <gabor.karsay@gmx.at>
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


#ifndef PT_WAVEVIEWER_CURSOR_H
#define PT_WAVEVIEWER_CURSOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVEVIEWER_CURSOR          (pt_waveviewer_cursor_get_type ())
#define PT_WAVEVIEWER_CURSOR(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_WAVEVIEWER_CURSOR, PtWaveviewerCursor))
#define PT_IS_WAVEVIEWER_CURSOR(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_WAVEVIEWER_CURSOR))
#define PT_WAVEVIEWER_CURSOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  PT_TYPE_WAVEVIEWER_CURSOR, PtWaveviewerCursorClass))
#define PT_IS_WAVEVIEWER_CURSOR_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PT_TYPE_WAVEVIEWER_CURSOR))

typedef struct _PtWaveviewerCursor		PtWaveviewerCursor;
typedef struct _PtWaveviewerCursorClass		PtWaveviewerCursorClass;
typedef struct _PtWaveviewerCursorPrivate	PtWaveviewerCursorPrivate;

struct _PtWaveviewerCursor {
	GtkDrawingArea parent;

	/*< private > */
	PtWaveviewerCursorPrivate *priv;
};

struct _PtWaveviewerCursorClass {
	GtkDrawingAreaClass klass;
};

GType		pt_waveviewer_cursor_get_type	(void) G_GNUC_CONST;
void		pt_waveviewer_cursor_render	(PtWaveviewerCursor *self,
						 gint                position);
void		pt_waveviewer_cursor_set_focus	(PtWaveviewerCursor *self,
						 gboolean            focus);
GtkWidget	*pt_waveviewer_cursor_new	(void);

G_END_DECLS

#endif // PT_WAVEVIEWER_CURSOR_H
