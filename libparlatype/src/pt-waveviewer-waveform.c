/* Copyright (C) 2017 Gabor Karsay <gabor.karsay@gmx.at>
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

/* PtWaveviewerWaveform is a GtkDrawingArea. It is part of a GtkOverlay stack.
   It renders only the waveform. */


#include "config.h"
#include "pt-waveviewer-waveform.h"


struct _PtWaveviewerWaveformPrivate {
	/* Wavedata */
	gfloat   *peaks;
	gint64    peaks_size;	/* size of array */

	/* Rendering */
	GdkRGBA   wave_color;
	gboolean  rtl;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewerWaveform, pt_waveviewer_waveform, GTK_TYPE_DRAWING_AREA);


static gint64
flip_pixel (PtWaveviewerWaveform *self,
            gint64                pixel)
{
	/* In ltr layouts each pixel on the x-axis corresponds to a sample in the array.
	   In rtl layouts this is flipped, e.g. the first pixel corresponds to
	   the last sample in the array. */

	gint64 samples;
	gint   widget_width;

	samples = self->priv->peaks_size / 2;
	widget_width = gtk_widget_get_allocated_width (GTK_WIDGET (self));

	/* Case: waveform is shorter than drawing area */
	if (samples < widget_width)
		return (widget_width - pixel);
	else
		return (samples - pixel);
}

static gint64
pixel_to_array (PtWaveviewerWaveform *self,
                gint64                pixel)
{
	/* Convert a position in the drawing area to an index in the peaks array.
	   The returned index is the peak’s min value, +1 is the max value.
	   If the array would be exceeded, return -1 */

	gint64 result;

	if (self->priv->rtl)
		pixel = flip_pixel (self, pixel);

	result = pixel * 2;

	if (result + 1 >= self->priv->peaks_size)
		result = -1;

	return result;
}

static gboolean
pt_waveviewer_waveform_draw (GtkWidget *widget,
                             cairo_t   *cr)
{
	PtWaveviewerWaveform *self = (PtWaveviewerWaveform *) widget;

	gfloat *peaks = self->priv->peaks;
	GtkStyleContext *context;
	gint pixel;
	gint array;
	gdouble min, max;
	gint width, height;
	gint half, middle;
	gdouble left, right;

	context = gtk_widget_get_style_context (widget);
	height = gtk_widget_get_allocated_height (widget);
	width = gtk_widget_get_allocated_width (widget);

	gtk_render_background (context, cr, 0, 0, width, height);
	if (!peaks)
		return FALSE;

	/* paint waveform */

	/* get extents, only render what we’re asked for */
	cairo_clip_extents (cr, &left, NULL, &right, NULL);
	half = height / 2 - 1;
	middle = height / 2;
	gdk_cairo_set_source_rgba (cr, &self->priv->wave_color);
	for (pixel = (gint)left; pixel <= (gint)right; pixel += 1) {
		array = pixel_to_array (self, pixel);
		if (array == -1)
			/* in ltr we could break, but rtl needs to continue */
			continue;
		min = (middle + half * peaks[array] * -1);
		max = (middle - half * peaks[array + 1]);
		cairo_move_to (cr, pixel, min);
		cairo_line_to (cr, pixel, max);
		/* cairo_stroke also possible after loop, but then slower */
		cairo_stroke (cr);
	}

	return FALSE;
}

static void
update_cached_style_values (PtWaveviewerWaveform *self)
{
	/* Update color and direction */

	GtkStyleContext *context;
	GtkStateFlags    state;
	GdkWindow       *window = NULL;

	window = gtk_widget_get_parent_window (GTK_WIDGET (self));
	if (!window)
		return;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	state = gtk_style_context_get_state (context);
	gtk_style_context_get_color (context, state, &self->priv->wave_color);

	if (state & GTK_STATE_FLAG_DIR_RTL)
		self->priv->rtl = TRUE;
	else
		self->priv->rtl = FALSE;
}

static void
pt_waveviewer_waveform_state_flags_changed (GtkWidget     *widget,
                                            GtkStateFlags  flags)
{
	update_cached_style_values (PT_WAVEVIEWER_WAVEFORM (widget));
	GTK_WIDGET_CLASS (pt_waveviewer_waveform_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_waveviewer_waveform_style_updated (GtkWidget *widget)
{
	PtWaveviewerWaveform *self = (PtWaveviewerWaveform *) widget;

	GTK_WIDGET_CLASS (pt_waveviewer_waveform_parent_class)->style_updated (widget);

	update_cached_style_values (self);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_waveform_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (pt_waveviewer_waveform_parent_class)->realize (widget);
	update_cached_style_values (PT_WAVEVIEWER_WAVEFORM (widget));
}

void
pt_waveviewer_waveform_set (PtWaveviewerWaveform *self,
                            gfloat               *peaks,
                            gint64                peaks_size)
{
	self->priv->peaks = peaks;
	self->priv->peaks_size = peaks_size;
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_waveform_init (PtWaveviewerWaveform *self)
{
	self->priv = pt_waveviewer_waveform_get_instance_private (self);

	GtkStyleContext *context;

	self->priv->peaks = NULL;
	self->priv->peaks_size = 0;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

	gtk_widget_set_events (GTK_WIDGET (self), GDK_ALL_EVENTS_MASK);
}

static void
pt_waveviewer_waveform_class_init (PtWaveviewerWaveformClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->draw                = pt_waveviewer_waveform_draw;
	widget_class->realize             = pt_waveviewer_waveform_realize;
	widget_class->state_flags_changed = pt_waveviewer_waveform_state_flags_changed;
	widget_class->style_updated       = pt_waveviewer_waveform_style_updated;
}

GtkWidget *
pt_waveviewer_waveform_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_WAVEFORM, NULL));
}
