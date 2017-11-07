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


#ifndef PT_WAVEVIEWER_FOCUS_H
#define PT_WAVEVIEWER_FOCUS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVEVIEWER_FOCUS          (pt_waveviewer_focus_get_type ())
#define PT_WAVEVIEWER_FOCUS(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_WAVEVIEWER_FOCUS, PtWaveviewerFocus))
#define PT_IS_WAVEVIEWER_FOCUS(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_WAVEVIEWER_FOCUS))
#define PT_WAVEVIEWER_FOCUS_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  PT_TYPE_WAVEVIEWER_FOCUS, PtWaveviewerFocusClass))
#define PT_IS_WAVEVIEWER_FOCUS_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PT_TYPE_WAVEVIEWER_FOCUS))

typedef struct _PtWaveviewerFocus		PtWaveviewerFocus;
typedef struct _PtWaveviewerFocusClass		PtWaveviewerFocusClass;
typedef struct _PtWaveviewerFocusPrivate	PtWaveviewerFocusPrivate;

struct _PtWaveviewerFocus {
	GtkDrawingArea parent;

	/*< private > */
	PtWaveviewerFocusPrivate *priv;
};

struct _PtWaveviewerFocusClass {
	GtkDrawingAreaClass klass;
};

GType		pt_waveviewer_focus_get_type	(void) G_GNUC_CONST;
void		pt_waveviewer_focus_set		(PtWaveviewerFocus *self,
						 gboolean           focus);
GtkWidget	*pt_waveviewer_focus_new	(void);

G_END_DECLS

#endif // PT_WAVEVIEWER_FOCUS_H
