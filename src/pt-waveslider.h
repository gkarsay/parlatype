/* Buzztrax
 * Copyright (C) 2008 Buzztrax team <buzztrax-devel@buzztrax.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
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

typedef struct _PtWaveslider      PtWaveslider;
typedef struct _PtWavesliderClass PtWavesliderClass;

/**
 * PtWaveslider:
 *
 * waveform view widget
 */
struct _PtWaveslider {
	GtkWidget parent;

	gfloat	 *peaks;
	gint	  peaks_size;
	gint	  channels;
    
	gint64	  wave_length;
	gint64	  playback_cursor;
  
	/* state */
	GdkWindow *window;
	GtkBorder  border;
};

struct _PtWavesliderClass {
	GtkWidgetClass klass;
};

GType		pt_waveslider_get_type	(void) G_GNUC_CONST;

void		pt_waveslider_set_wave	(PtWaveslider *self,
					 gint16	      *data,
					 gint	       channels,
					 gint	       length);

GtkWidget	*pt_waveslider_new	(void);



G_END_DECLS

#endif // PT_WAVESLIDER_H
