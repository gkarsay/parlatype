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

#include <string.h>
#include "pt-waveslider.h"


#define CURSOR_POSITION 0.5
#define MARKER_BOX_W 6
#define MARKER_BOX_H 5
#define MIN_W 24
#define MIN_H 16


struct _PtWavesliderPrivate {
	gfloat	 *peaks;
	gint64	  peaks_size;
	gint	  px_per_sec;

	gint64	  wave_length;
	gint64	  playback_cursor;

	GdkRGBA	  wave_color;
	GdkRGBA	  cursor_color;

	/* state */
	GdkWindow *window;
	GtkBorder  border;
};

enum
{
	PROP_0,
	PROP_PLAYBACK_CURSOR,
	N_PROPERTIES
};

enum {
	CURSOR_CHANGED,
	LAST_SIGNAL
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveslider, pt_waveslider, GTK_TYPE_WIDGET);

/**
 * SECTION: pt-waveslider
 * @short_description: A GtkWidget to display a waveform.
 * @include: parlatype-1.0/pt-waveslider.h
 *
 * Displays a waveform provided by PtWaveloader or PtPlayer.
 */


static void
pt_waveslider_realize (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	GdkWindow *window;
	GtkAllocation allocation;
	GdkWindowAttr attributes;
	gint attributes_mask;

	gtk_widget_set_realized (widget, TRUE);

	window = gtk_widget_get_parent_window (widget);
	gtk_widget_set_window (widget, window);
	g_object_ref (window);

	gtk_widget_get_allocation (widget, &allocation);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= (GDK_EXPOSURE_MASK |
		GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK |
		GDK_BUTTON_MOTION_MASK |
		GDK_ENTER_NOTIFY_MASK |
		GDK_LEAVE_NOTIFY_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	attributes_mask = GDK_WA_X | GDK_WA_Y;

	self->priv->window = gdk_window_new (window, &attributes, attributes_mask);
	gtk_widget_register_window (widget, self->priv->window);
}

static void
pt_waveslider_unrealize (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	gtk_widget_unregister_window (widget, self->priv->window);
	gdk_window_destroy (self->priv->window);
	self->priv->window = NULL;
	GTK_WIDGET_CLASS (pt_waveslider_parent_class)->unrealize (widget);
}

static void
pt_waveslider_map (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	gdk_window_show (self->priv->window);

	GTK_WIDGET_CLASS (pt_waveslider_parent_class)->map (widget);
}

static void
pt_waveslider_unmap (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	gdk_window_hide (self->priv->window);

	GTK_WIDGET_CLASS (pt_waveslider_parent_class)->unmap (widget);
}

static gint64
time_to_pixel (gint64 ms, gint pix_per_sec)
{
	/* Convert a time in milliseconds to the closest position in samples array */
	gint64 result;

	result = ms * pix_per_sec;
	result = result / 1000;

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

static gint
get_left_pixel (PtWaveslider *self,
		gint width,
	        gint cursor_pixel)
{
	gint left;
	gint last_pixel;

	last_pixel = self->priv->peaks_size / 2;
	left = cursor_pixel - width * CURSOR_POSITION;

	if (width * CURSOR_POSITION > cursor_pixel)
		left = 0;

	if (width * (1 - CURSOR_POSITION) > last_pixel - cursor_pixel)
		left = last_pixel - width;

	if (last_pixel < width)
		left = 0;

	return left;
}

static gboolean
pt_waveslider_draw (GtkWidget *widget,
		    cairo_t   *cr)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	GtkStyleContext *style_ctx;
	gint width, height, left, top, middle, half;
	gint i, x;
	gfloat *peaks = self->priv->peaks;
	gdouble min, max;
	gint offset;

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);
	style_ctx = gtk_widget_get_style_context (widget);

	/* draw border */
	gtk_render_background (style_ctx, cr, 0, 0, width, height);
	gtk_render_frame (style_ctx, cr, 0, 0, width, height);

	if (!peaks) {
		g_debug ("draw, no peaks");
		return FALSE;
	}

	left = self->priv->border.left;
	top = self->priv->border.top;
	width -= self->priv->border.left + self->priv->border.right;
	height -= self->priv->border.top + self->priv->border.bottom;
	middle = top + height / 2;
	half = height / 2 - 1;

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 1.0);

	gint cursor_pixel = time_to_pixel (self->priv->playback_cursor * 10, self->priv->px_per_sec);
	/* offset = pixel at the left in peaks array */
	offset = get_left_pixel (self, width, cursor_pixel) * 2;

	/* waveform */
	for (i = 0; i < 2 * width; i += 2) {
		if (offset + i < 0)
			continue;
		if (offset + i > self->priv->peaks_size)
			break;
		gint x = left + i / 2;
		min = (middle + half * peaks[offset + i + 1] * -1);
		max = (middle - half * peaks[offset + i + 2]);
		cairo_move_to (cr, x, min);
		cairo_line_to (cr, x, max);
	}

	gdk_cairo_set_source_rgba (cr, &self->priv->wave_color);
	cairo_stroke (cr);

	/* cursor */
	gdk_cairo_set_source_rgba (cr, &self->priv->cursor_color);
	x = left + cursor_pixel - offset / 2;
	if (x > left + width)
		x = left + width;
	cairo_move_to (cr, x, top + height);
	cairo_line_to (cr, x, top);
	cairo_stroke (cr);
	cairo_move_to (cr, x, top + height / 2 - MARKER_BOX_H);
	cairo_line_to (cr, x, top + height / 2 + MARKER_BOX_H);
	cairo_line_to (cr, x + MARKER_BOX_W, top + height / 2);
	cairo_line_to (cr, x, top + height / 2 - MARKER_BOX_H);
	cairo_fill (cr);

	return FALSE;
}

static void
pt_waveslider_size_allocate (GtkWidget	   *widget,
			     GtkAllocation *allocation)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	GtkStyleContext *context;
	GtkStateFlags state;

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget))
		gdk_window_move_resize (self->priv->window,
			allocation->x, allocation->y, allocation->width, allocation->height);

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_get_border (context, state, &self->priv->border);
}

static void
pt_waveslider_get_preferred_width (GtkWidget *widget,
				   gint	     *minimal_width,
				   gint	     *natural_width)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	gint border_padding = self->priv->border.left + self->priv->border.right;

	*minimal_width = MIN_W + border_padding;
	*natural_width = (MIN_W * 6) + border_padding;
}

static void
pt_waveslider_get_preferred_height (GtkWidget *widget,
				    gint      *minimal_height,
				    gint      *natural_height)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	gint border_padding = self->priv->border.top + self->priv->border.bottom;

	*minimal_height = MIN_H + border_padding;
	*natural_height = (MIN_H * 4) + border_padding;
}

static gboolean
pt_waveslider_button_press (GtkWidget	   *widget,
			    GdkEventButton *event)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	const gint width = gtk_widget_get_allocated_width (widget) - 2;

	gint64 cursor_pixel;
	gint64 left;	/* the first sample in the view */
	gint64 clicked;	/* the sample clicked on */
	gint64 pos;	/* clicked sample's position in milliseconds */

	cursor_pixel = time_to_pixel (self->priv->playback_cursor * 10, self->priv->px_per_sec);
	left = get_left_pixel (self, width, cursor_pixel);
	clicked = left + (gint) event->x;
	pos = pixel_to_time (clicked, self->priv->px_per_sec);

	g_debug ("left: %" G_GINT64_FORMAT, left);
	g_debug ("clicked: %" G_GINT64_FORMAT, clicked);
	g_debug ("pos: %" G_GINT64_FORMAT, pos);

	if (clicked < 0 || clicked > self->priv->peaks_size / 2) {
		g_debug ("click outside");
		return FALSE;
	}

	/* Single left click */
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY) {
		g_signal_emit_by_name (widget, "cursor-changed", pos);
		g_debug ("click, jump to: %" G_GINT64_FORMAT "ms", pos);
		return TRUE;
	}

	return FALSE;
}

static gboolean
pt_waveslider_button_release (GtkWidget	     *widget,
			      GdkEventButton *event)
{
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
	style_ctx = gtk_widget_get_style_context (widget);

	if (gdk_window_get_state (window) & GDK_WINDOW_STATE_FOCUSED) {
		gtk_style_context_lookup_color (style_ctx, "wave_color", &self->priv->wave_color);
		gtk_style_context_lookup_color (style_ctx, "cursor_color", &self->priv->cursor_color);
	} else {
		gtk_style_context_lookup_color (style_ctx, "wave_color_uf", &self->priv->wave_color);
		gtk_style_context_lookup_color (style_ctx, "cursor_color_uf", &self->priv->cursor_color);
	}
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
	switch (property_id) {
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
		self->priv->playback_cursor = g_value_get_int64 (value);
		if (gtk_widget_get_realized (GTK_WIDGET (self))) {
			gtk_widget_queue_draw (GTK_WIDGET (self));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveslider_class_init (PtWavesliderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->realize = pt_waveslider_realize;
	widget_class->unrealize = pt_waveslider_unrealize;
	widget_class->map = pt_waveslider_map;
	widget_class->unmap = pt_waveslider_unmap;
	widget_class->draw = pt_waveslider_draw;
	widget_class->get_preferred_width = pt_waveslider_get_preferred_width;
	widget_class->get_preferred_height = pt_waveslider_get_preferred_height;
	widget_class->size_allocate = pt_waveslider_size_allocate;
	widget_class->button_press_event = pt_waveslider_button_press;
	widget_class->button_release_event = pt_waveslider_button_release;
	widget_class->state_flags_changed = pt_waveslider_state_flags_changed;

	gobject_class->set_property = pt_waveslider_set_property;
	gobject_class->get_property = pt_waveslider_get_property;
	gobject_class->finalize = pt_waveslider_finalize;

	/**
	* PtWaveslider::cursor-changed:
	* @ws: the waveslider emitting the signal
	* @position: the new position in stream in milliseconds
	*
	* Signals that the cursor's position has changed.
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

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

static void
pt_waveslider_init (PtWaveslider *self)
{
	self->priv = pt_waveslider_get_instance_private (self);

	GtkStyleContext *context;
	GtkCssProvider  *provider;
	GtkSettings     *settings;
	gboolean	 dark;
	GFile		*file;

	self->priv->peaks_size = 0;
	self->priv->playback_cursor = 0;

	gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

	settings = gtk_settings_get_default ();
	g_object_get (settings, "gtk-application-prefer-dark-theme", &dark, NULL);

	if (dark)
		file = g_file_new_for_uri ("resource:///org/gnome/libparlatype/pt-waveslider-dark.css");
	else
		file = g_file_new_for_uri ("resource:///org/gnome/libparlatype/pt-waveslider.css");

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_file (provider, file, NULL);

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
	gtk_style_context_add_provider (context,
					GTK_STYLE_PROVIDER (provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_style_context_lookup_color (context, "wave_color", &self->priv->wave_color);
	gtk_style_context_lookup_color (context, "cursor_color", &self->priv->cursor_color);

	g_object_unref (file);
	g_object_unref (provider);
}

/**
 * pt_waveslider_set_wave:
 * @self: the widget
 * @data: memory block of samples, min and max value for each sample
 * @length: number of elements in data array
 * @px_per_sec: how many peaks/pixels are one second
 *
 * Set wave data to show in the widget.
 */
void
pt_waveslider_set_wave (PtWaveslider *self,
			gfloat       *data,
			gint64	      length,
			gint	      px_per_sec)
{
	g_free (self->priv->peaks);
	self->priv->peaks = NULL;

	if (!data || !length) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		return;
	}

	self->priv->px_per_sec = px_per_sec;
	self->priv->peaks_size = length;
	self->priv->peaks = g_malloc (sizeof (gfloat) * self->priv->peaks_size);
	self->priv->peaks = data;

	gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * pt_waveslider_new:
 *
 * Create a new waveform viewer widget. Use pt_waveslider_set_wave() to
 * pass wave data.
 *
 * Returns: the widget
 */
GtkWidget *
pt_waveslider_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVESLIDER, NULL));
}
