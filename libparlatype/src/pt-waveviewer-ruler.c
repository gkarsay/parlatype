/* Copyright (C) 2017, 2020 Gabor Karsay <gabor.karsay@gmx.at>
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

/**
 * pt-waveviewer-ruler
 * Internal widget that draws the ruler for PtWaveviewer.
 *
 * When it's added to PtWaveviewer, it gets the horizontal GtkAdjustment from
 * its parent (listening for the hierarchy-changed signal).
 *
 * Use pt_waveviewer_ruler_set_ruler() to set it up. @n_samples is the number
 * of lines in the waveform or the width of the fully plotted waveform.
 * @px_per_sec is how many pixels are worth one second and @duration is the
 * duration in ms of the audio stream.
 *
 * The height of the ruler is determined by the height of the system font plus
 * 5px for marks and 6px for padding.
 *
 * Marks (primary and secondary) are computed based on font size and pixel per
 * second ratio.
 *
 * It listens to changes of the parent's horizontal GtkAdjustment and redraws
 * itself (scroll movements, size changes).
 *
 * The widget listens to the style-updated signal (e.g. changed font size, color)
 * and the state-flags-changed signal to update its height/color.
 *
 * The widget has a GTK_STYLE_CLASS_MARK and a name "ruler".
 */


#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include "pt-waveviewer.h"
#include "pt-waveviewer-ruler.h"


#define PRIMARY_MARK_HEIGHT 8
#define SECONDARY_MARK_HEIGHT 4

struct _PtWaveviewerRulerPrivate {
	gint64    n_samples;
	gint      px_per_sec;
	gint64    duration;	/* in milliseconds */

	GtkAdjustment *adj;	/* the parent PtWaveviewer’s adjustment */

	/* Ruler marks */
	gboolean  time_format_long;
	gint      time_string_width;
	gint      primary_modulo;
	gint      secondary_modulo;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewerRuler, pt_waveviewer_ruler, GTK_TYPE_DRAWING_AREA);


static gint64
time_to_pixel (PtWaveviewerRuler *self,
               gint64             ms)
{
	/* Convert a time in 1/1000 seconds to the closest pixel in the drawing area */
	gint64 result;

	result = ms * self->priv->px_per_sec;
	result = result / 1000;

	return result;
}

static gint64
pixel_to_time (PtWaveviewerRuler *self,
               gint64             pixel)
{
	/* Convert a position in the drawing area to time in milliseconds */
	gint64 result;

	result = pixel * 1000;
	result = result / self->priv->px_per_sec;

	return result;
}

static gboolean
pt_waveviewer_ruler_draw (GtkWidget *widget,
                          cairo_t   *cr)
{
	PtWaveviewerRuler *self = (PtWaveviewerRuler *) widget;
	gint height = gtk_widget_get_allocated_height (widget);

	gint            i;		/* counter, pixel on x-axis in the view */
	gint            sample;		/* sample in the array */
	gchar          *text;
	PangoLayout    *layout;
	PangoRectangle  rect;
	gint            halfwidth;
	gint64          tmp_time;
	GtkStyleContext *context;
	GdkRGBA         text_color;
	gint            width;
	gint            offset;

	width = gtk_widget_get_allocated_width (widget);
	offset = (gint) gtk_adjustment_get_value (self->priv->adj);

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_render_background (context, cr, 0, 0, width, height);

	if (self->priv->n_samples == 0)
		return FALSE;

	/* ruler marks */

	/* Case: secondary ruler marks for tenth seconds.
	   Get time of leftmost pixel, convert it to rounded 10th second,
	   add 10th seconds until we are at the end of the view */
	if (self->priv->primary_modulo == 1) {
		tmp_time = pixel_to_time (self, offset) / 100 * 100;
		i = time_to_pixel (self, tmp_time) - offset;
		while (i <= width) {
			if (tmp_time < self->priv->duration)
				gtk_render_line (context, cr, i, 0, i, SECONDARY_MARK_HEIGHT);
			tmp_time += 100;
			i = time_to_pixel (self, tmp_time) - offset;
		}
	}

	/* Case: secondary ruler marks for full seconds.
	   Use secondary_modulo. */
	if (self->priv->primary_modulo > 1) {
		for (i = 0; i <= width; i += 1) {
			sample = i + offset;
			if (sample > self->priv->n_samples)
				break;
			if (sample % (self->priv->px_per_sec * self->priv->secondary_modulo) == 0)
				gtk_render_line (context, cr, i, 0, i, SECONDARY_MARK_HEIGHT);
		}
	}

	/* Primary marks and time strings
	   Add some padding to show time strings (time_string_width) */
	gtk_style_context_get_color (context, &text_color);
	gdk_cairo_set_source_rgba (cr, &text_color);
	for (i = 0 - self->priv->time_string_width; i <= width + self->priv->time_string_width; i += 1) {
		sample = i + offset;
		if (sample < 0 || sample > self->priv->n_samples)
			continue;
		if (sample % (self->priv->px_per_sec * self->priv->primary_modulo) == 0) {
			gtk_render_line (context, cr, i, 0, i, PRIMARY_MARK_HEIGHT);
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

			/* display timestring only if it is fully visible in drawing area */
			pango_layout_get_pixel_extents (layout, &rect, NULL);
			halfwidth = rect.width / 2;
			if (i - halfwidth > 0 && i + halfwidth < width) {
				cairo_move_to (cr,
					       i - halfwidth,	/* center at mark */
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
calculate_height (PtWaveviewerRuler *self)
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
	if (!window || self->priv->n_samples == 0) {
		gtk_widget_set_size_request (GTK_WIDGET (self), 0, 0);
		return;
	}

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
		/* In fact this would be 0.1, it’s handled later as a special case */
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

	gtk_widget_set_size_request (GTK_WIDGET (self), -1, ruler_height);
}

static void
pt_waveviewer_ruler_style_updated (GtkWidget *widget)
{
	PtWaveviewerRuler *self = PT_WAVEVIEWER_RULER (widget);

	GTK_WIDGET_CLASS (pt_waveviewer_ruler_parent_class)->style_updated (widget);

	calculate_height (self);
	gtk_widget_queue_draw (widget);
}

static void
adj_value_changed (GtkAdjustment *adj,
                   gpointer       data)
{
	PtWaveviewerRuler *self = PT_WAVEVIEWER_RULER (data);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_ruler_hierarchy_changed (GtkWidget *widget,
                                       GtkWidget *old_parent)
{
	PtWaveviewerRuler *self = PT_WAVEVIEWER_RULER (widget);

	if (self->priv->adj)
		return;

	/* Get parent’s GtkAdjustment */
	GtkWidget *parent = NULL;
	parent = gtk_widget_get_ancestor (widget, GTK_TYPE_SCROLLED_WINDOW);
	if (parent) {
		self->priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (parent));
		g_signal_connect (self->priv->adj, "value-changed", G_CALLBACK (adj_value_changed), self);
	}
}

void
pt_waveviewer_ruler_set_ruler (PtWaveviewerRuler *self,
                               gint64             n_samples,
                               gint               px_per_sec,
                               gint64             duration)
{
	self->priv->n_samples = n_samples;
	self->priv->px_per_sec = px_per_sec;
	self->priv->duration = duration;

	calculate_height (self);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_ruler_init (PtWaveviewerRuler *self)
{
	self->priv = pt_waveviewer_ruler_get_instance_private (self);

	GtkStyleContext *context;

	self->priv->n_samples = 0;
	self->priv->px_per_sec = 0;
	self->priv->duration = 0;
	self->priv->adj = NULL;

	gtk_widget_set_name (GTK_WIDGET (self), "ruler");
	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_MARK);
}

static void
pt_waveviewer_ruler_class_init (PtWaveviewerRulerClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->draw                = pt_waveviewer_ruler_draw;
	widget_class->hierarchy_changed   = pt_waveviewer_ruler_hierarchy_changed;
	widget_class->style_updated       = pt_waveviewer_ruler_style_updated;
}

GtkWidget *
pt_waveviewer_ruler_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_RULER, NULL));
}
