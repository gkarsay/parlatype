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


#ifndef PT_RULER_H
#define PT_RULER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_RULER          (pt_ruler_get_type ())
#define PT_RULER(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_RULER, PtRuler))
#define PT_IS_RULER(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_RULER))
#define PT_RULER_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  PT_TYPE_RULER, PtRulerClass))
#define PT_IS_RULER_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PT_TYPE_RULER))

typedef struct _PtRuler		PtRuler;
typedef struct _PtRulerClass	PtRulerClass;
typedef struct _PtRulerPrivate	PtRulerPrivate;

struct _PtRuler {
	GtkDrawingArea parent;

	/*< private > */
	PtRulerPrivate *priv;
};

struct _PtRulerClass {
	GtkDrawingAreaClass klass;
};

GType		pt_ruler_get_type	(void) G_GNUC_CONST;
void		pt_ruler_set_ruler	(PtRuler *self,
			                 gint64   n_samples,
			                 gint     px_per_sec,
			                 gint64   duration);
GtkWidget	*pt_ruler_new		(void);


G_END_DECLS

#endif // PT_RULER_H
