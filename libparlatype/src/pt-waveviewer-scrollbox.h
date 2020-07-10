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


#ifndef PT_WAVEVIEWER_SCROLLBOX_H
#define PT_WAVEVIEWER_SCROLLBOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVEVIEWER_SCROLLBOX          (pt_waveviewer_scrollbox_get_type ())
#define PT_WAVEVIEWER_SCROLLBOX(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_WAVEVIEWER_SCROLLBOX, PtWaveviewerScrollbox))
#define PT_IS_WAVEVIEWER_SCROLLBOX(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_WAVEVIEWER_SCROLLBOX))
#define PT_WAVEVIEWER_SCROLLBOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  PT_TYPE_WAVEVIEWER_SCROLLBOX, PtWaveviewerScrollboxClass))
#define PT_IS_WAVEVIEWER_SCROLLBOX_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PT_TYPE_WAVEVIEWER_SCROLLBOX))

typedef struct _PtWaveviewerScrollbox		PtWaveviewerScrollbox;
typedef struct _PtWaveviewerScrollboxClass	PtWaveviewerScrollboxClass;
typedef struct _PtWaveviewerScrollboxPrivate	PtWaveviewerScrollboxPrivate;

struct _PtWaveviewerScrollbox {
	GtkBox parent;

	/*< private > */
	PtWaveviewerScrollboxPrivate *priv;
};

struct _PtWaveviewerScrollboxClass {
	GtkBoxClass klass;
};

GType		pt_waveviewer_scrollbox_get_type	(void) G_GNUC_CONST;

void		pt_waveviewer_scrollbox_set	(PtWaveviewerScrollbox *self,
						 gint                   width);

GtkWidget	*pt_waveviewer_scrollbox_new	(void);

G_END_DECLS

#endif // PT_WAVEVIEWER_SCROLLBOX_H
