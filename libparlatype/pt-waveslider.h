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


#ifndef PT_WAVESLIDER_H
#define PT_WAVESLIDER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVESLIDER          (pt_waveslider_get_type ())
#define PT_WAVESLIDER(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_WAVESLIDER, PtWaveslider))
#define PT_IS_WAVESLIDER(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_WAVESLIDER))
#define PT_WAVESLIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  PT_TYPE_WAVESLIDER, PtWavesliderClass))
#define PT_IS_WAVESLIDER_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  PT_TYPE_WAVESLIDER))

typedef struct _PtWaveslider		PtWaveslider;
typedef struct _PtWavesliderClass	PtWavesliderClass;
typedef struct _PtWavesliderPrivate	PtWavesliderPrivate;

/**
 * PtWaveslider:
 *
 * The #PtWaveslider contains only private fields and should not be directly accessed.
 */
struct _PtWaveslider {
	GtkScrolledWindow parent;

	/*< private > */
	PtWavesliderPrivate *priv;
};

struct _PtWavesliderClass {
	GtkScrolledWindowClass klass;
};

GType		pt_waveslider_get_type	(void) G_GNUC_CONST;

gboolean	pt_waveslider_get_follow_cursor (PtWaveslider *self);

void		pt_waveslider_set_follow_cursor (PtWaveslider *self,
						 gboolean      follow);

void		pt_waveslider_set_wave	(PtWaveslider *self,
					 gfloat	      *data,
					 gint64        length,
					 gint	       px_per_sec);

GtkWidget	*pt_waveslider_new	(void);



G_END_DECLS

#endif // PT_WAVESLIDER_H
