/* Buzztrax
 * Copyright (C) 2006-2008 Buzztrax team <buzztrax-devel@buzztrax.org>
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
/**
 * SECTION:btwaveformviewer
 * @short_description: the waveform viewer widget
 * @see_also: #BtWave, #BtMainPageWaves
 *
 * Provides an viewer for audio waveforms. It can handle multi-channel
 * waveforms, show loop-markers and a playback cursor.
 */
/* TODO(ensonic): add selection support
 * - export the selection as two properties
 */

#include <string.h>
#include "waveform-viewer.h"

enum
{
  WAVE_VIEWER_NULL,
  WAVE_VIEWER_PLAYBACK_CURSOR
};

#define MARKER_BOX_W 6
#define MARKER_BOX_H 5
#define MIN_W 24
#define MIN_H 16

#define DEF_PEAK_SIZE 1000

//-- the class

G_DEFINE_TYPE (BtWaveformViewer, bt_waveform_viewer, GTK_TYPE_WIDGET);


static void
bt_waveform_viewer_realize (GtkWidget *widget)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);
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

	self->window = gdk_window_new (window, &attributes, attributes_mask);
	gtk_widget_register_window (widget, self->window);
}

static void
bt_waveform_viewer_unrealize (GtkWidget *widget)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);

	gtk_widget_unregister_window (widget, self->window);
	gdk_window_destroy (self->window);
	self->window = NULL;
	GTK_WIDGET_CLASS (bt_waveform_viewer_parent_class)->unrealize (widget);
}

static void
bt_waveform_viewer_map (GtkWidget *widget)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);

	gdk_window_show (self->window);

	GTK_WIDGET_CLASS (bt_waveform_viewer_parent_class)->map (widget);
}

static void
bt_waveform_viewer_unmap (GtkWidget *widget)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);

	gdk_window_hide (self->window);

	GTK_WIDGET_CLASS (bt_waveform_viewer_parent_class)->unmap (widget);
}

static gboolean
bt_waveform_viewer_draw (GtkWidget *widget,
			 cairo_t   *cr)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);
	GtkStyleContext *style_ctx;
	gint width, height, left, top, middle, half;
	gint i, x;
	gfloat *peaks = self->peaks;
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

	left = self->border.left;
	top = self->border.top;
	width -= self->border.left + self->border.right;
	height -= self->border.top + self->border.bottom;
	middle = top + height / 2;
	half = height / 2 - 1;

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 1.0);

	//gtk_style_context_lookup_color (style_ctx, "wave_color", &wave_color);
	//gtk_style_context_lookup_color (style_ctx, "peak_color", &peak_color);
	GdkRGBA wave_color = {0.3, 0.3, 0.3, 1.0};
	GdkRGBA peak_color = {0.8, 0.8, 0.8, 1.0};
	GdkRGBA line_color = {1, 0, 0, 1};

	offset = self->playback_cursor * 2 - width;

	/* before waveform */
	gdk_cairo_set_source_rgba (cr, &peak_color);
	if (offset < 0) {
		cairo_rectangle (cr, left, top, offset /2 * -1, height);
		cairo_fill (cr);
	}

	/* beyond waveform */
	gint diff = offset + width * 2 - self->peaks_size;
	if (diff > 0) {
		cairo_rectangle (cr, left + width - diff / 2, top, diff / 2, height);
		cairo_fill (cr);
	}

	/* waveform */
	for (i = 0; i < 2 * width; i += 2) {
		if (offset + i < 0)
			continue;
		if (offset + i > self->peaks_size)
			break;
		gint x = left + i / 2;
		min = (middle + half * peaks[offset + i + 1] * -1);
		max = (middle - half * peaks[offset + i + 2]);
		cairo_move_to (cr, x, min);
		cairo_line_to (cr, x, max);
	}

	gdk_cairo_set_source_rgba (cr, &wave_color);
	cairo_stroke (cr);

	/* cursor */
	if (self->playback_cursor != -1) {
		//gtk_style_context_lookup_color (style_ctx, "playline_color", &line_color);
		gdk_cairo_set_source_rgba (cr, &line_color);
		x = (gint) (left + width / 2) - 1;
		cairo_move_to (cr, x, top + height);
		cairo_line_to (cr, x, top);
		cairo_stroke (cr);
		cairo_move_to (cr, x, top + height / 2 - MARKER_BOX_H);
		cairo_line_to (cr, x, top + height / 2 + MARKER_BOX_H);
		cairo_line_to (cr, x + MARKER_BOX_W, top + height / 2);
		cairo_line_to (cr, x, top + height / 2 - MARKER_BOX_H);
		cairo_fill (cr);
	}

	return FALSE;
}

static void
bt_waveform_viewer_size_allocate (GtkWidget	*widget,
				  GtkAllocation *allocation)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);
	GtkStyleContext *context;
	GtkStateFlags state;

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget))
		gdk_window_move_resize (self->window,
			allocation->x, allocation->y, allocation->width, allocation->height);

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_get_border (context, state, &self->border);
}

static void
bt_waveform_viewer_get_preferred_width (GtkWidget *widget,
					gint	  *minimal_width,
					gint	  *natural_width)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);
	gint border_padding = self->border.left + self->border.right;

	*minimal_width = MIN_W + border_padding;
	*natural_width = (MIN_W * 6) + border_padding;
}

static void
bt_waveform_viewer_get_preferred_height (GtkWidget *widget,
					 gint	   *minimal_height,
					 gint	   *natural_height)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);
	gint border_padding = self->border.top + self->border.bottom;

	*minimal_height = MIN_H + border_padding;
	*natural_height = (MIN_H * 4) + border_padding;
}

static gboolean
bt_waveform_viewer_button_press (GtkWidget	*widget,
				 GdkEventButton *event)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);
	const gint ox = 1, oy = 1;
	const gint sx = gtk_widget_get_allocated_width (widget) - 2;

	return FALSE;
}

static gboolean
bt_waveform_viewer_button_release (GtkWidget	  *widget,
				   GdkEventButton *event)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);

	return FALSE;
}

static gboolean
bt_waveform_viewer_motion_notify (GtkWidget	 *widget,
				  GdkEventMotion *event)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (widget);
	const gint ox = 1;
	const gint sx = gtk_widget_get_allocated_width (widget) - 2;
	gint64 pos = (event->x - ox) * (gdouble) self->wave_length / sx;

	pos = CLAMP (pos, 0, self->wave_length);

	return FALSE;
}

static void
bt_waveform_viewer_finalize (GObject *object)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (object);

	g_free (self->peaks);

	G_OBJECT_CLASS (bt_waveform_viewer_parent_class)->finalize (object);
}

static void
bt_waveform_viewer_get_property (GObject    *object,
				 guint	     property_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (object);

	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
  }
}

static void
bt_waveform_viewer_set_property (GObject      *object,
				 guint	       property_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	BtWaveformViewer *self = BT_WAVEFORM_VIEWER (object);

	switch (property_id) {
	case WAVE_VIEWER_PLAYBACK_CURSOR:
		self->playback_cursor = g_value_get_int64 (value) / 441;
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
bt_waveform_viewer_class_init (BtWaveformViewerClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->realize = bt_waveform_viewer_realize;
	widget_class->unrealize = bt_waveform_viewer_unrealize;
	widget_class->map = bt_waveform_viewer_map;
	widget_class->unmap = bt_waveform_viewer_unmap;
	widget_class->draw = bt_waveform_viewer_draw;
	widget_class->get_preferred_width = bt_waveform_viewer_get_preferred_width;
	widget_class->get_preferred_height = bt_waveform_viewer_get_preferred_height;
	widget_class->size_allocate = bt_waveform_viewer_size_allocate;
	widget_class->button_press_event = bt_waveform_viewer_button_press;
	widget_class->button_release_event = bt_waveform_viewer_button_release;
	widget_class->motion_notify_event = bt_waveform_viewer_motion_notify;

	gobject_class->set_property = bt_waveform_viewer_set_property;
	gobject_class->get_property = bt_waveform_viewer_get_property;
	gobject_class->finalize = bt_waveform_viewer_finalize;

	g_object_class_install_property (gobject_class, WAVE_VIEWER_PLAYBACK_CURSOR,
		g_param_spec_int64 ("playback-cursor",
			"playback cursor position",
			"Current playback position within a waveform or -1 if sample is not played",
			-1, G_MAXINT64, -1, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void
bt_waveform_viewer_init (BtWaveformViewer *self)
{
	GtkStyleContext *context;

	self->channels = 2;
	self->peaks_size = DEF_PEAK_SIZE;
	self->peaks = g_malloc (sizeof (gfloat) * self->channels * self->peaks_size);
	self->wave_length = 0;
	self->playback_cursor = -1;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

	gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

/**
 * bt_waveform_viewer_set_wave:
 * @self: the widget
 * @data: memory block of samples (interleaved for channels>1)
 * @channels: number channels
 * @length: number samples per channel
 *
 * Set wave data to show in the widget.
 */
void
bt_waveform_viewer_set_wave (BtWaveformViewer *self,
			     gint16	      *data,
			     gint	       channels,
			     gint	       length)
{
	/* Create 100 data pairs (minimum and maximum value) per second
	   from raw data. Input must be mono, at a bit rate of 44100.
	   Move this later to the waveloader. */

	gint i, p;
	gint64 len = length; /* number of samples */
	gint rate = 44100;   /* for reference */

	self->wave_length = length;

	g_free (self->peaks);
	self->peaks = NULL;

	if (!data || !length) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		return;
	}

	/* calculate peak data */
	self->peaks_size = length / 441 * 2;
	self->peaks = g_malloc (sizeof (gfloat) * self->peaks_size - 2);

	for (i = 0; i < self->peaks_size -2 ; i += 2) {
		gint p1 = len * i / self->peaks_size;
		gint p2 = len * (i + 1) / self->peaks_size;

		/* get min max for peak slot */
		gfloat vmin = data[p1 + 1], vmax = data[p1 + 1];
		for (p = p1 + 1; p < p2; p++) {
			gfloat d = data[p + 1];
			if (d < vmin)
				vmin = d;
			if (d > vmax)
				vmax = d;
		}
		if (vmin > 0 && vmax > 0)
			vmin = 0;
		else if (vmin < 0 && vmax < 0)
			vmax = 0;
		self->peaks[i + 1] = vmin / 32768.0;
		self->peaks[i + 2] = vmax / 32768.0;
	}

	gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * bt_waveform_viewer_new:
 *
 * Create a new waveform viewer widget. Use bt_waveform_viewer_set_wave() to
 * pass wave data.
 *
 * Returns: the widget
 */
GtkWidget *
bt_waveform_viewer_new (void)
{
	return GTK_WIDGET (g_object_new (BT_TYPE_WAVEFORM_VIEWER, NULL));
}
