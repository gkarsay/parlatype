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

#include "config.h"
#define GETTEXT_PACKAGE PACKAGE
#include <glib/gi18n-lib.h>
#include "pt-ruler.h"


#define PRIMARY_MARK_HEIGHT 8
#define SECONDARY_MARK_HEIGHT 4

struct _PtRulerPrivate {
	gboolean  rtl;
	gint64	  n_samples;
	gint	  px_per_sec;
	gint64    duration;	/* in milliseconds */

	/* Ruler marks */
	gboolean  time_format_long;
	gint      time_string_width;
	gint      primary_modulo;
	gint      secondary_modulo;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtRuler, pt_ruler, GTK_TYPE_DRAWING_AREA);


static gint64
flip_pixel (PtRuler *self,
	    gint64   pixel)
{
	/* In ltr layouts each pixel on the x-axis corresponds to a sample in the array.
	   In rtl layouts this is flipped, e.g. the first pixel corresponds to
	   the last sample in the array. */

	gint   widget_width;

	widget_width = gtk_widget_get_allocated_width (GTK_WIDGET (self));

	/* Case: waveform is shorter than drawing area */
	if (self->priv->n_samples < widget_width)
		return (widget_width - pixel);
	else
		return (self->priv->n_samples - pixel);
}

static gint64
time_to_pixel (PtRuler *self,
	       gint64   ms)
{
	/* Convert a time in 1/1000 seconds to the closest pixel in the drawing area */
	gint64 result;

	result = ms * self->priv->px_per_sec;
	result = result / 1000;

	if (self->priv->rtl)
		result = flip_pixel (self, result);

	return result;
}

static gint64
pixel_to_time (PtRuler *self,
	       gint64   pixel)
{
	/* Convert a position in the drawing area to time in milliseconds */
	gint64 result;

	if (self->priv->rtl)
		pixel = flip_pixel (self, pixel);

	result = pixel * 1000;
	result = result / self->priv->px_per_sec;

	return result;
}

static gboolean
ruler_draw_cb (GtkWidget *widget,
               cairo_t   *cr,
               gpointer   data)
{
	PtRuler *self = (PtRuler *) data;
	gint height = gtk_widget_get_allocated_height (widget);

	gint	        i;		/* counter, pixel on x-axis in the view */
	gint		sample;		/* sample in the array */
	gchar          *text;
	PangoLayout    *layout;
	PangoRectangle  rect;
	gint            x_offset;
	gint64          tmp_time;
	GtkStyleContext *context;
	GdkRGBA         text_color;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_render_background (context, cr,
                               0, 0,
                               self->priv->n_samples, height);
	
	if (self->priv->n_samples == 0)
		return FALSE;

	/* ruler marks */

	/* Case: secondary ruler marks for tenth seconds.
	   Get time of leftmost pixel, convert it to rounded 10th second,
	   add 10th seconds until we are at the end of the view */
	if (self->priv->primary_modulo == 1) {
		tmp_time = pixel_to_time (self, 0) / 100 * 100;
		if (self->priv->rtl)
			tmp_time += 100; /* round up */
		i = time_to_pixel (self, tmp_time);
		while (i <= self->priv->n_samples) {
			if (tmp_time < self->priv->duration)
				gtk_render_line (context, cr, i, 0, i, SECONDARY_MARK_HEIGHT);
			if (self->priv->rtl)
				tmp_time -= 100;
			else
				tmp_time += 100;
			i = time_to_pixel (self, tmp_time);
		}
	}

	/* Case: secondary ruler marks for full seconds.
	   Use secondary_modulo. */
	if (self->priv->primary_modulo > 1) {
		for (i = 0; i <= self->priv->n_samples; i += 1) {
			sample = i;
			if (self->priv->rtl)
				sample = flip_pixel (self, sample);
			if (sample > self->priv->n_samples)
				continue;
			if (sample % (self->priv->px_per_sec * self->priv->secondary_modulo) == 0)
				gtk_render_line (context, cr, i, 0, i, SECONDARY_MARK_HEIGHT);
		}
	}

	/* Primary marks and time strings */
	for (i = 0; i <= self->priv->n_samples; i += 1) {
		sample = i;
		if (self->priv->rtl)
			sample = flip_pixel (self, sample);
		if (sample > self->priv->n_samples)
			continue;
		if (sample % (self->priv->px_per_sec * self->priv->primary_modulo) == 0) {
			gtk_render_line (context, cr, i, 0, i, PRIMARY_MARK_HEIGHT);
			gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &text_color);
			gdk_cairo_set_source_rgba (cr, &text_color);
			if (self->priv->time_format_long) {
				text = g_strdup_printf (C_("long time format", "%d:%02d:%02d"),
							sample/self->priv->px_per_sec / 3600,
							(sample/self->priv->px_per_sec % 3600) / 60,
							sample/self->priv->px_per_sec % 60);
			} else {
				text = g_strdup_printf (C_("shortest time format", "%d:%02d"),
							sample/self->priv->px_per_sec/60,
							sample/self->priv->px_per_sec % 60);
			}
			layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), text);
			pango_cairo_update_layout (cr, layout);
			pango_layout_get_pixel_extents (layout, &rect, NULL);
			x_offset = (rect.x + rect.width) / 2;

			/* don't display time if it is not fully visible in drawing area */
			if (i - x_offset > 0 && i + x_offset < gtk_widget_get_allocated_width (widget)) {
				cairo_move_to (cr,
					       i - x_offset,	/* center at mark */
					       height - rect.y - rect.height -3); /* +3 px above border */
				pango_cairo_show_layout (cr, layout);
			}
			g_free (text);
			g_object_unref (layout);
		}
	}
	return FALSE;
}

static void
pt_ruler_calculate_height (PtRuler *self)
{
	/* Calculate ruler height and time string width*/
	cairo_t         *cr;
	cairo_surface_t *surface;
	PangoLayout     *layout;
	PangoRectangle   rect;
	gchar           *time_format;
	gint             ruler_height;
	GdkWindow       *window = NULL;

	window = gtk_widget_get_parent_window (GTK_WIDGET (self));
	if (!window || self->priv->n_samples == 0)
		return;

	surface = gdk_window_create_similar_surface (window,
	                                             CAIRO_CONTENT_COLOR,
	                                             100, 100);
	cr = cairo_create (surface);

	self->priv->time_format_long = (self->priv->n_samples / self->priv->px_per_sec >= 3600);

	if (self->priv->time_format_long)
		time_format = g_strdup_printf (C_("long time format", "%d:%02d:%02d"), 88, 0, 0);
	else
		time_format = g_strdup_printf (C_("shortest time format", "%d:%02d"), 88, 0);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), time_format);
	pango_cairo_update_layout (cr, layout);
	pango_layout_get_pixel_extents (layout, &rect, NULL);

	/* Ruler height = font height + 3px padding below + 3px padding above + 5px for marks */
	ruler_height = rect.y + rect.height + 3 + 3 + 5;

	/* Does the time string fit into 1 second?
	   Define primary and secondary modulo (for primary and secondary marks */
	self->priv->time_string_width = rect.x + rect.width;
	if (self->priv->time_string_width < self->priv->px_per_sec) {
		self->priv->primary_modulo = 1;
		/* In fact this would be 0.1, it's handled later as a special case */
		self->priv->secondary_modulo = 1;
	} else if (self->priv->time_string_width < self->priv->px_per_sec * 5) {
		self->priv->primary_modulo = 5;
		self->priv->secondary_modulo = 1;
	} else if (self->priv->time_string_width < self->priv->px_per_sec * 10) {
		self->priv->primary_modulo = 10;
		self->priv->secondary_modulo = 1;
	} else if (self->priv->time_string_width < self->priv->px_per_sec * 60) {
		self->priv->primary_modulo = 60;
		self->priv->secondary_modulo = 10;
	} else if (self->priv->time_string_width < self->priv->px_per_sec * 300) {
		self->priv->primary_modulo = 300;
		self->priv->secondary_modulo = 60;
	} else if (self->priv->time_string_width < self->priv->px_per_sec * 600) {
		self->priv->primary_modulo = 600;
		self->priv->secondary_modulo = 60;
	} else {
		self->priv->primary_modulo = 3600;
		self->priv->secondary_modulo = 600;
	}

	g_free (time_format);
	g_object_unref (layout);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	gtk_widget_set_size_request (GTK_WIDGET (self), self->priv->n_samples, ruler_height);
}

static void
pt_ruler_update_cached_style_values (PtRuler *self)
{
	/* Direction is cached */

	GtkStyleContext *context;
	GdkWindow       *window = NULL;

	window = gtk_widget_get_parent_window (GTK_WIDGET (self));
	if (!window)
		return;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));

	if (gtk_style_context_get_state (context) & GTK_STATE_FLAG_DIR_RTL)
		self->priv->rtl = TRUE;
	else
		self->priv->rtl = FALSE;
}

static void
pt_ruler_state_flags_changed (GtkWidget	    *widget,
                              GtkStateFlags  flags)
{
	PtRuler *self = PT_RULER (widget);
	pt_ruler_update_cached_style_values (self);

	GTK_WIDGET_CLASS (pt_ruler_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_ruler_style_updated (GtkWidget *widget)
{
	PtRuler *self = PT_RULER (widget);

	GTK_WIDGET_CLASS (pt_ruler_parent_class)->style_updated (widget);

	pt_ruler_calculate_height (self);
	pt_ruler_update_cached_style_values (self);
	gtk_widget_queue_draw (widget);
}

void
pt_ruler_set_ruler (PtRuler *self,
                    gint64   n_samples,
                    gint     px_per_sec,
                    gint64   duration)
{
	self->priv->n_samples = n_samples;
	self->priv->px_per_sec = px_per_sec;
	self->priv->duration = duration;

	pt_ruler_calculate_height (self);
	pt_ruler_update_cached_style_values (self);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_ruler_init (PtRuler *self)
{
	self->priv = pt_ruler_get_instance_private (self);

	GtkStyleContext *context;

	self->priv->n_samples = 0;
	self->priv->px_per_sec = 0;
	self->priv->duration = 0;

	gtk_widget_set_name (GTK_WIDGET (self), "ruler");
	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_MARK);

	g_signal_connect (self,
			  "draw",
			  G_CALLBACK (ruler_draw_cb),
			  self);
}

static void
pt_ruler_class_init (PtRulerClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->state_flags_changed = pt_ruler_state_flags_changed;
	widget_class->style_updated       = pt_ruler_style_updated;
}

GtkWidget *
pt_ruler_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_RULER, NULL));
}
