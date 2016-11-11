/* Copyright (C) 2006-2008 Buzztrax team <buzztrax-devel@buzztrax.org>
 * Copyright (C) Gabor Karsay 2016 <gabor.karsay@gmx.at>
 *
 * Original source name waveform-viewer.c, taken from Buzztrax and modified.
 * Original source licenced under LGPL 2 or later. As it's currently not a
 * library anymore this file is licenced under the GPL 3 or later.
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
#include <string.h>
#define GETTEXT_PACKAGE PACKAGE
#include <glib/gi18n-lib.h>
#include "pt-waveslider.h"


#define CURSOR_POSITION 0.5
#define MARKER_BOX_W 6
#define MARKER_BOX_H 5
#define WAVE_MIN_HEIGHT 20


struct _PtWavesliderPrivate {
	gfloat	 *peaks;
	gint64	  peaks_size;
	gint	  px_per_sec;

	gint64	  playback_cursor;
	gboolean  follow_cursor;
	gboolean  fixed_cursor;
	gboolean  show_ruler;

	GdkRGBA	  wave_color;
	GdkRGBA	  cursor_color;
	GdkRGBA	  ruler_color;
	GdkRGBA	  mark_color;

	GtkWidget       *drawarea;
	GtkAdjustment   *adj;
	cairo_surface_t *cursor;

	gboolean  time_format_long;
	gint      time_string_width;
	gint      ruler_height;
	gint      primary_modulo;
	gint      secondary_modulo;
};

enum
{
	PROP_0,
	PROP_PLAYBACK_CURSOR,
	PROP_FOLLOW_CURSOR,
	PROP_FIXED_CURSOR,
	PROP_SHOW_RULER,
	N_PROPERTIES
};

enum {
	CURSOR_CHANGED,
	LAST_SIGNAL
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveslider, pt_waveslider, GTK_TYPE_SCROLLED_WINDOW);


/**
 * SECTION: pt-waveslider
 * @short_description: A GtkWidget to display a waveform.
 * @stability: Unstable
 * @include: parlatype/pt-waveslider.h
 *
 * Displays a waveform provided by #PtPlayer or #PtWaveloader.
 */


static gint64
time_to_pixel (gint64 ms, gint pix_per_sec)
{
	/* Convert a time in 1/100 seconds to the closest position in samples array */
	gint64 result;

	result = ms * pix_per_sec;
	result = result / 100;

	return result;
}

static gint64
pixel_to_time (gint64 pixel, gint pix_per_sec)
{
	/* Convert a position in samples array to time in milliseconds */
	gint64 result;

	result = pixel * 1000;
	result = result / pix_per_sec;

	return result;
}

static void
draw_cursor (PtWaveslider *self)
{
	if (!gtk_widget_get_realized (GTK_WIDGET (self)))
		return;

	if (self->priv->cursor)
		cairo_surface_destroy (self->priv->cursor);

	cairo_t *cr;
	gint height = gtk_widget_get_allocated_height (self->priv->drawarea);


	self->priv->cursor = gdk_window_create_similar_surface (gtk_widget_get_window (GTK_WIDGET (self)),
	                                             CAIRO_CONTENT_COLOR_ALPHA,
	                                             MARKER_BOX_W,
	                                             height);

	cr = cairo_create (self->priv->cursor);

	gdk_cairo_set_source_rgba (cr, &self->priv->cursor_color);

	cairo_move_to (cr, 0, height);
	cairo_line_to (cr, 0, 0);
	cairo_stroke (cr);
	cairo_move_to (cr, 0, height / 2 - MARKER_BOX_H);
	cairo_line_to (cr, 0, height / 2 + MARKER_BOX_H);
	cairo_line_to (cr, 0 + MARKER_BOX_W, height / 2);
	cairo_line_to (cr, 0, height / 2 - MARKER_BOX_H);
	cairo_fill (cr);

	cairo_destroy (cr);
}

static void
scroll_to_cursor (PtWaveslider *self)
{
	gint cursor_pos;
	gint page_width;
	gint offset;

	cursor_pos = time_to_pixel (self->priv->playback_cursor, self->priv->px_per_sec);
	page_width = (gint) gtk_adjustment_get_page_size (self->priv->adj);
	offset = page_width * CURSOR_POSITION;

	gtk_adjustment_set_value (self->priv->adj, cursor_pos - offset);
}

static void
size_allocate_cb (GtkWidget     *widget,
                  GtkAllocation *rectangle,
                  gpointer       data)
{
	/* When size of widget changed, we have to change the cursor's size, too */

	PtWaveslider *self = (PtWaveslider *) data;
	draw_cursor (self);
}

static void
paint_ruler (PtWaveslider *self,
	     cairo_t      *cr,
	     gint          height,
	     gint          visible_first,
	     gint          visible_last)
{
	gint	        i;
	gchar          *text;
	PangoLayout    *layout;
	PangoRectangle  rect;
	gint            x_offset;
	gint            tmp_time;

	/* ruler background */
	gdk_cairo_set_source_rgba (cr, &self->priv->ruler_color);
	cairo_rectangle (cr, 0, height, visible_last, self->priv->ruler_height);
	cairo_fill (cr);

	/* ruler marks */
	gdk_cairo_set_source_rgba (cr, &self->priv->mark_color);

	/* Case: secondary ruler marks for tenth seconds.
	   Get time of leftmost pixel, convert it to rounded 10th second,
	   add 10th seconds until we are at the end of the view */
	if (self->priv->primary_modulo == 1) {
		tmp_time = pixel_to_time (visible_first, self->priv->px_per_sec) / 100 * 10;
		i = time_to_pixel (tmp_time, self->priv->px_per_sec);
		while (i <= visible_last) {
			cairo_move_to (cr, i, height);
			cairo_line_to (cr, i, height + 4);
			cairo_stroke (cr);
			tmp_time += 10;
			i = time_to_pixel (tmp_time, self->priv->px_per_sec);
		}
	}

	/* Case: secondary ruler marks for full seconds.
	   Use secondary_modulo. */
	if (self->priv->primary_modulo > 1) {
		for (i = visible_first; i <= visible_last; i += 1) {
			if (i % (self->priv->px_per_sec * self->priv->secondary_modulo) == 0) {
				cairo_move_to (cr, i, height);
				cairo_line_to (cr, i, height + 4);
				cairo_stroke (cr);
			}
		}
	}

	/* Primary marks and time strings */
	for (i = visible_first; i <= visible_last; i += 1) {
		if (i % (self->priv->px_per_sec * self->priv->primary_modulo) == 0) {
			gdk_cairo_set_source_rgba (cr, &self->priv->mark_color);
			cairo_move_to (cr, i, height);
			cairo_line_to (cr, i, height + 8);
			cairo_stroke (cr);
			gdk_cairo_set_source_rgba (cr, &self->priv->wave_color);
			if (self->priv->time_format_long) {
				text = g_strdup_printf (C_("long time format", "%d:%02d:%02d"),
							i/self->priv->px_per_sec / 3600,
							(i/self->priv->px_per_sec % 3600) / 60,
							i/self->priv->px_per_sec % 60);
			} else {
				text = g_strdup_printf (C_("shortest time format", "%d:%02d"),
							i/self->priv->px_per_sec/60,
							i/self->priv->px_per_sec % 60);
			}
			layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), text);
			pango_cairo_update_layout (cr, layout);
			pango_layout_get_pixel_extents (layout, &rect, NULL);
			x_offset = (rect.x + rect.width) / 2;

			/* don't display time if it is not fully visible */
			if (i - x_offset > 0 && i + x_offset < self->priv->peaks_size / 2) {
				cairo_move_to (cr,
					       i - x_offset,	/* center at mark */
					       height + self->priv->ruler_height - rect.y - rect.height -3); /* +3 px above border */
				pango_cairo_show_layout (cr, layout);
			}
			cairo_stroke (cr);
			g_free (text);
			g_object_unref (layout);
		}
	}
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
	/* Redraw screen, cairo_t is already clipped to only draw the exposed
	   areas of the drawing area */

	g_debug ("draw_cb");

	PtWaveslider *self = (PtWaveslider *) data;

	gfloat *peaks = self->priv->peaks;
	if (!peaks)
		return FALSE;

	gint visible_first;
	gint visible_last;

	gint i;
	gdouble min, max;

	gint height = gtk_widget_get_allocated_height (widget);

	if (self->priv->show_ruler)
		height = height - self->priv->ruler_height;

	gint half = height / 2 - 1;
	gint middle = height / 2;

	visible_first = (gint) gtk_adjustment_get_value (self->priv->adj);
	visible_last = visible_first + (gint) gtk_adjustment_get_page_size (self->priv->adj);

	g_debug ("visible area: %d–%d", visible_first, visible_last);

	gdk_cairo_set_source_rgba (cr, &self->priv->wave_color);

	/* paint waveform */
	for (i = visible_first; i <= visible_last; i += 1) {
		min = (middle + half * peaks[i * 2] * -1);
		max = (middle - half * peaks[i * 2 + 1]);
		cairo_move_to (cr, i, min);
		cairo_line_to (cr, i, max);
		/* cairo_stroke also possible after loop, but then slower */
		cairo_stroke (cr);
	}

	/* paint ruler */
	if (self->priv->show_ruler)
		paint_ruler (self, cr, height, visible_first, visible_last);


	/* paint cursor */
	cairo_set_source_surface (cr,
	                          self->priv->cursor,
	                          time_to_pixel (self->priv->playback_cursor,
	                                         self->priv->px_per_sec),
	                          0);
	cairo_paint (cr);

	return FALSE;
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
		       GdkEventButton *event,
		       gpointer        data)
{
	PtWaveslider *slider = PT_WAVESLIDER (data);

	if (!slider->priv->peaks)
		return FALSE;

	gint64 clicked;	/* the sample clicked on */
	gint64 pos;	/* clicked sample's position in milliseconds */

	clicked = (gint) event->x;
	pos = pixel_to_time (clicked, slider->priv->px_per_sec);

	/* Single left click */
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY) {
		g_signal_emit_by_name (slider, "cursor-changed", pos);
		return TRUE;
	}

	return FALSE;
}

static gboolean
motion_notify_event_cb (GtkWidget      *widget,
                        GdkEventMotion *event,
                        gpointer        data)
{
	PtWaveslider *slider = PT_WAVESLIDER (data);

	if (!slider->priv->peaks)
		return FALSE;

	gint64 clicked;	/* the sample clicked on */
	gint64 pos;	/* clicked sample's position in milliseconds */

	clicked = (gint) event->x;
	pos = pixel_to_time (clicked, slider->priv->px_per_sec);

	if (event->state & GDK_BUTTON1_MASK) {
		g_signal_emit_by_name (slider, "cursor-changed", pos);
		return TRUE;
	}

	return FALSE;
}

static void
pt_waveslider_state_flags_changed (GtkWidget	 *widget,
				   GtkStateFlags  flags)
{
	/* Change colors depending on window state focused/not focused */

	PtWaveslider *self = PT_WAVESLIDER (widget);

	GtkStyleContext *style_ctx;
	GdkWindow *window;

	window = gtk_widget_get_parent_window (widget);
	style_ctx = gtk_widget_get_style_context (self->priv->drawarea);

	if (gdk_window_get_state (window) & GDK_WINDOW_STATE_FOCUSED) {
		gtk_style_context_lookup_color (style_ctx, "wave_color", &self->priv->wave_color);
		gtk_style_context_lookup_color (style_ctx, "cursor_color", &self->priv->cursor_color);
		gtk_style_context_lookup_color (style_ctx, "ruler_color", &self->priv->ruler_color);
	} else {
		gtk_style_context_lookup_color (style_ctx, "wave_color_uf", &self->priv->wave_color);
		gtk_style_context_lookup_color (style_ctx, "cursor_color_uf", &self->priv->cursor_color);
		gtk_style_context_lookup_color (style_ctx, "ruler_color_uf", &self->priv->ruler_color);
	}

	draw_cursor (self);
}

static gboolean
scroll_child_cb (GtkScrolledWindow *self,
                 GtkScrollType      scroll,
                 gboolean           horizontal,
                 gpointer           data)
{
	PtWaveslider *slider = PT_WAVESLIDER (data);

	if (!horizontal)
		return FALSE;

	if (scroll == GTK_SCROLL_PAGE_BACKWARD ||
	    scroll == GTK_SCROLL_PAGE_FORWARD ||
	    scroll == GTK_SCROLL_PAGE_LEFT ||
	    scroll == GTK_SCROLL_PAGE_RIGHT ||
	    scroll == GTK_SCROLL_STEP_BACKWARD ||
	    scroll == GTK_SCROLL_STEP_FORWARD ||
	    scroll == GTK_SCROLL_STEP_LEFT ||
	    scroll == GTK_SCROLL_STEP_RIGHT ||
	    scroll == GTK_SCROLL_START ||
	    scroll == GTK_SCROLL_END ) {
		pt_waveslider_set_follow_cursor (slider, FALSE);
	}

	return FALSE;
}

static void
adj_cb (GtkAdjustment *adj,
	gpointer      *data)
{
	g_debug ("adjustment changed");

	/* GtkScrolledWindow draws itself mostly automatically, but some
	   adjustment changes are not propagated for reasons I don't understand.
	   Probably we're doing some draws twice */
	PtWaveslider *self = PT_WAVESLIDER (data);
	gtk_widget_queue_draw (GTK_WIDGET (self->priv->drawarea));
}

static gboolean
scrollbar_cb (GtkWidget      *widget,
	      GdkEventButton *event,
	      gpointer        data)
{
	/* If user clicks on scrollbar don't follow cursor anymore.
	   Otherwise it would scroll immediately back again. */

	PtWaveslider *slider = PT_WAVESLIDER (data);
	pt_waveslider_set_follow_cursor (slider, FALSE);

	/* Propagate signal */
	return FALSE;
}

/**
 * pt_waveslider_get_follow_cursor:
 * @self: the widget
 *
 * Get follow-cursor option.
 *
 * Return value: TRUE if cursor is followed, else FALSE
 */
gboolean
pt_waveslider_get_follow_cursor (PtWaveslider *self)
{
	g_return_val_if_fail (PT_IS_WAVESLIDER (self), FALSE);

	return self->priv->follow_cursor;
}

/**
 * pt_waveslider_set_follow_cursor:
 * @self: the widget
 * @follow: new value
 *
 * Set follow-cursor option to TRUE or FALSE. See also #PtWaveslider:follow-cursor.
 */
void
pt_waveslider_set_follow_cursor (PtWaveslider *self,
				 gboolean      follow)
{
	g_return_if_fail (PT_IS_WAVESLIDER (self));

	if (self->priv->follow_cursor != follow) {
		self->priv->follow_cursor = follow;
		g_object_notify_by_pspec (G_OBJECT (self),
					  obj_properties[PROP_FOLLOW_CURSOR]);
	}
	if (follow)
		scroll_to_cursor (self);
}

/**
 * pt_waveslider_set_wave:
 * @self: the widget
 * @data: (allow-none): a #PtWavedata
 *
 * Set wave data to show in the widget. The data is copied internally and may
 * be freed immediately after calling this function. If @data is NULL, a blank
 * widget is drawn.
 */
void
pt_waveslider_set_wave (PtWaveslider *self,
			PtWavedata   *data)
{
	g_return_if_fail (PT_IS_WAVESLIDER (self));

	g_debug ("set_wave");

	if (self->priv->peaks) {
		g_free (self->priv->peaks);
		self->priv->peaks = NULL;
	}

	if (!data) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		return;
	}

	if (!data->array || !data->length) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		return;
	}

	/* Copy array. If wiget is not owner of its data, bad things can happen
	   with bindings. */
	if (!(self->priv->peaks = g_malloc (sizeof (gfloat) * data->length))) {
		g_debug	("waveslider failed to allocate memory");
		gtk_widget_queue_draw (GTK_WIDGET (self));
		return;
	}

	memcpy (self->priv->peaks, data->array, sizeof(gfloat) * data->length);
	self->priv->peaks_size = data->length;
	self->priv->px_per_sec = data->px_per_sec;

	/* Calculate ruler height and time string width*/
	cairo_t         *cr;
	cairo_surface_t *surface;
	PangoLayout     *layout;
	PangoRectangle   rect;
	gchar           *time_format;

	surface = gdk_window_create_similar_surface (gtk_widget_get_window (GTK_WIDGET (self)),
	                                             CAIRO_CONTENT_COLOR,
	                                             100, 100);
	cr = cairo_create (surface);

	self->priv->time_format_long = (self->priv->peaks_size / 2 / self->priv->px_per_sec >= 3600);

	if (self->priv->time_format_long)
		time_format = g_strdup_printf (C_("long time format", "%d:%02d:%02d"), 88, 0, 0);
	else
		time_format = g_strdup_printf (C_("shortest time format", "%d:%02d"), 88, 0);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), time_format);
	pango_cairo_update_layout (cr, layout);
	pango_layout_get_pixel_extents (layout, &rect, NULL);

	/* Ruler height = font height + 3px padding below + 3px padding above + 5px for marks */
	self->priv->ruler_height = rect.y + rect.height + 3 + 3 + 5;

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

	if (self->priv->show_ruler)
		gtk_widget_set_size_request (self->priv->drawarea, data->length / 2, WAVE_MIN_HEIGHT + self->priv->ruler_height);
	else
		gtk_widget_set_size_request (self->priv->drawarea, data->length / 2, WAVE_MIN_HEIGHT);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveslider_finalize (GObject *object)
{
	PtWaveslider *self = PT_WAVESLIDER (object);

	g_free (self->priv->peaks);

	G_OBJECT_CLASS (pt_waveslider_parent_class)->finalize (object);
}

static void
pt_waveslider_get_property (GObject    *object,
			    guint       property_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	PtWaveslider *self = PT_WAVESLIDER (object);

	switch (property_id) {
	case PROP_FOLLOW_CURSOR:
		g_value_set_boolean (value, self->priv->follow_cursor);
		break;
	case PROP_FIXED_CURSOR:
		g_value_set_boolean (value, self->priv->fixed_cursor);
		break;
	case PROP_SHOW_RULER:
		g_value_set_boolean (value, self->priv->show_ruler);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveslider_set_property (GObject      *object,
			    guint	  property_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	PtWaveslider *self = PT_WAVESLIDER (object);

	switch (property_id) {
	case PROP_PLAYBACK_CURSOR:
		if (self->priv->playback_cursor == g_value_get_int64 (value))
			break;
		self->priv->playback_cursor = g_value_get_int64 (value);
		if (gtk_widget_get_realized (GTK_WIDGET (self))) {
			if (self->priv->follow_cursor)
				scroll_to_cursor (self);
			gtk_widget_queue_draw (GTK_WIDGET (self->priv->drawarea));
		}
		break;
	case PROP_FOLLOW_CURSOR:
		self->priv->follow_cursor = g_value_get_boolean (value);
		break;
	case PROP_FIXED_CURSOR:
		self->priv->fixed_cursor = g_value_get_boolean (value);
		break;
	case PROP_SHOW_RULER:
		self->priv->show_ruler = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveslider_constructed (GObject *object)
{
	g_debug ("waveslider constructed");

	PtWaveslider *self = PT_WAVESLIDER (object);
	self->priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (self));
	g_signal_connect (self->priv->adj, "value-changed", G_CALLBACK (adj_cb), self);

	g_signal_connect (gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (self)),
			  "button_press_event",
			  G_CALLBACK (scrollbar_cb),
			  self);
}

static void
pt_waveslider_init (PtWaveslider *self)
{
	self->priv = pt_waveslider_get_instance_private (self);

	gtk_widget_init_template (GTK_WIDGET (self));

	GtkStyleContext *context;
	GtkCssProvider  *provider;
	GtkSettings     *settings;
	gboolean	 dark;
	GFile		*file;

	self->priv->peaks = NULL;
	self->priv->peaks_size = 0;
	self->priv->playback_cursor = 0;
	self->priv->follow_cursor = TRUE;

	settings = gtk_settings_get_default ();
	g_object_get (settings, "gtk-application-prefer-dark-theme", &dark, NULL);

	if (dark)
		file = g_file_new_for_uri ("resource:///org/gnome/libparlatype/pt-waveslider-dark.css");
	else
		file = g_file_new_for_uri ("resource:///org/gnome/libparlatype/pt-waveslider.css");

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_file (provider, file, NULL);

	context = gtk_widget_get_style_context (GTK_WIDGET (self->priv->drawarea));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
	gtk_style_context_add_provider (context,
					GTK_STYLE_PROVIDER (provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_style_context_lookup_color (context, "wave_color", &self->priv->wave_color);
	gtk_style_context_lookup_color (context, "cursor_color", &self->priv->cursor_color);
	gtk_style_context_lookup_color (context, "ruler_color", &self->priv->ruler_color);
	gtk_style_context_lookup_color (context, "mark_color", &self->priv->mark_color);

	g_object_unref (file);
	g_object_unref (provider);

	g_signal_connect (GTK_SCROLLED_WINDOW (self),
			  "scroll-child",
			  G_CALLBACK (scroll_child_cb),
			  self);

	g_signal_connect (self->priv->drawarea,
			  "button-press-event",
			  G_CALLBACK (button_press_event_cb),
			  self);

	g_signal_connect (self->priv->drawarea,
			  "draw",
			  G_CALLBACK (draw_cb),
			  self);

	g_signal_connect (self->priv->drawarea,
			  "motion-notify-event",
			  G_CALLBACK (motion_notify_event_cb),
			  self);

	g_signal_connect (self->priv->drawarea,
			  "size-allocate",
			  G_CALLBACK (size_allocate_cb),
			  self);

	gtk_widget_set_events (self->priv->drawarea, gtk_widget_get_events (self->priv->drawarea)
                                     | GDK_BUTTON_PRESS_MASK
				     | GDK_POINTER_MOTION_MASK);
}

static void
pt_waveslider_class_init (PtWavesliderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = pt_waveslider_set_property;
	gobject_class->get_property = pt_waveslider_get_property;
	gobject_class->constructed = pt_waveslider_constructed;
	gobject_class->finalize = pt_waveslider_finalize;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/libparlatype/ptwaveslider.ui");
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWaveslider, drawarea);
	widget_class->state_flags_changed = pt_waveslider_state_flags_changed;


	/**
	* PtWaveslider::cursor-changed:
	* @ws: the waveslider emitting the signal
	* @position: the new position in stream in milliseconds
	*
	* Signals that the cursor's position was changed by the user.
	*/
	signals[CURSOR_CHANGED] =
	g_signal_new ("cursor-changed",
		      PT_TYPE_WAVESLIDER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__INT,
		      G_TYPE_NONE,
		      1, G_TYPE_INT64);

	/**
	* PtWaveslider:playback-cursor:
	*
	* Current playback position in 1/100 seconds.
	*/

	obj_properties[PROP_PLAYBACK_CURSOR] =
	g_param_spec_int64 (
			"playback-cursor",
			"playback cursor position",
			"Current playback position within a waveform",
			0,
			G_MAXINT64,
			0,
			G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

	/**
	* PtWaveslider:follow-cursor:
	*
	* If the widget follows the cursor, it scrolls automatically to the
	* cursor's position. Note that the widget will change this property to
	* FALSE if the user scrolls the widget manually.
	*/

	obj_properties[PROP_FOLLOW_CURSOR] =
	g_param_spec_boolean (
			"follow-cursor",
			"follow cursor",
			"Scroll automatically to current cursor position",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	/**
	* PtWaveslider:fixed-cursor:
	*
	* If TRUE, in follow-cursor mode the cursor is at a fixed position and the
	* waveform is scrolling. If FALSE the cursor is moving.
	*
	* If #PtWaveslider:follow-cursor is FALSE, this has no effect.
	*/

	obj_properties[PROP_FIXED_CURSOR] =
	g_param_spec_boolean (
			"fixed-cursor",
			"fixed cursor",
			"In follow-cursor mode the cursor is at a fixed position",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	/**
	* PtWaveslider:show-ruler:
	*
	* Whether the ruler is shown (TRUE) or not (FALSE).
	*/

	obj_properties[PROP_SHOW_RULER] =
	g_param_spec_boolean (
			"show-ruler",
			"show ruler",
			"Show the ruler with time marks",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

/**
 * pt_waveslider_new:
 *
 * Create a new, initially blank waveform viewer widget. Use
 * pt_waveslider_set_wave() to pass wave data.
 *
 * After use gtk_widget_destroy() it.
 *
 * Returns: the widget
 */
GtkWidget *
pt_waveslider_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVESLIDER, NULL));
}
