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
#include <math.h>	/* fabs */
#include <string.h>
#define GETTEXT_PACKAGE PACKAGE
#include <glib/gi18n-lib.h>
#include "pt-waveslider.h"


#define ALL_ACCELS_MASK	(GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)
#define CURSOR_POSITION 0.5
#define MARKER_BOX_W 10
#define MARKER_BOX_H 8
#define WAVE_MIN_HEIGHT 20

struct _PtWavesliderPrivate {
	gfloat	 *peaks;
	gint64	  peaks_size;
	gint	  px_per_sec;

	gint64	  playback_cursor;
	gboolean  follow_cursor;
	gboolean  fixed_cursor;
	gboolean  show_ruler;

	gint64    sel_start;
	gint64    sel_end;
	gint64    dragstart;
	gint64    dragend;
	GdkCursor *arrows;

	GdkRGBA	  wave_color;
	GdkRGBA	  cursor_color;
	GdkRGBA	  ruler_color;
	GdkRGBA	  mark_color;

	GtkWidget       *drawarea;
	GtkAdjustment   *adj;
	cairo_surface_t *cursor;

	gboolean  rtl;
	gboolean  focus_on_cursor;

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
flip_pixel (PtWaveslider *self,
	    gint64        pixel)
{
	/* In ltr layouts each pixel on the x-axis corresponds to a sample in the array.
	   In rtl layouts this is flipped, e.g. the first pixel corresponds to
	   the last sample in the array. */

	return (self->priv->peaks_size / 2 - pixel);
}

static gint64
time_to_pixel (PtWaveslider *self,
	       gint64 ms)
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
pixel_to_time (PtWaveslider *self,
	       gint64 pixel)
{
	/* Convert a position in the drawing area to time in milliseconds */
	gint64 result;

	if (self->priv->rtl)
		pixel = flip_pixel (self, pixel);

	result = pixel * 1000;
	result = result / self->priv->px_per_sec;

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
	if (self->priv->show_ruler)
		height = height - self->priv->ruler_height;


	self->priv->cursor = gdk_window_create_similar_surface (gtk_widget_get_window (GTK_WIDGET (self)),
	                                             CAIRO_CONTENT_COLOR_ALPHA,
	                                             MARKER_BOX_W,
	                                             height);

	cr = cairo_create (self->priv->cursor);

	gdk_cairo_set_source_rgba (cr, &self->priv->cursor_color);

	cairo_move_to (cr, 0 + MARKER_BOX_W / 2, height);
	cairo_line_to (cr, 0 + MARKER_BOX_W / 2, 0);
	cairo_stroke (cr);
	cairo_move_to (cr, 0, 0);
	cairo_line_to (cr, 0 + MARKER_BOX_W, 0);
	cairo_line_to (cr, 0 + MARKER_BOX_W / 2 , 0 + MARKER_BOX_H);
	cairo_line_to (cr, 0, 0);
	cairo_fill (cr);

	cairo_destroy (cr);
}

static void
scroll_to_cursor (PtWaveslider *self)
{
	gint cursor_pos;
	gint first_visible;
	gint last_visible;
	gint page_width;
	gint offset;

	cursor_pos = time_to_pixel (self, self->priv->playback_cursor);
	first_visible = (gint) gtk_adjustment_get_value (self->priv->adj);
	page_width = (gint) gtk_adjustment_get_page_size (self->priv->adj);
	last_visible = first_visible + page_width;

	/* Fixed cursor: always scroll,
	   non-fixed cursor: only scroll if cursor is not visible */

	if (self->priv->fixed_cursor) {
		offset = page_width * CURSOR_POSITION;
		gtk_adjustment_set_value (self->priv->adj, cursor_pos - offset);
	} else {
		if (cursor_pos < first_visible || cursor_pos > last_visible) {
			if (self->priv->rtl)
				/* cursor visible far right */
				gtk_adjustment_set_value (self->priv->adj, cursor_pos - page_width);
			else
				/* cursor visible far left */
				gtk_adjustment_set_value (self->priv->adj, cursor_pos);
		}
	}
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

static gint64
pixel_to_array (PtWaveslider *self,
		gint64	      pixel)
{
	/* Convert a position in the drawing area to an index in the peaks array.
	   The returned index is the peak's min value, +1 is the max value */

	if (self->priv->rtl)
		pixel = flip_pixel (self, pixel);

	return (pixel * 2);
}

static void
paint_ruler (PtWaveslider *self,
	     cairo_t      *cr,
	     gint          height,
	     gint          visible_first,
	     gint          visible_last)
{
	gint	        i;		/* counter, pixel on x-axis in the view */
	gint		sample;		/* sample in the array */
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
		tmp_time = pixel_to_time (self, visible_first) / 100 * 100;
		if (self->priv->rtl)
			tmp_time += 100; /* round up */
		i = time_to_pixel (self, tmp_time);
		while (i <= visible_last) {
			cairo_move_to (cr, i, height);
			cairo_line_to (cr, i, height + 4);
			cairo_stroke (cr);
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
		for (i = visible_first; i <= visible_last; i += 1) {
			sample = i;
			if (self->priv->rtl)
				sample = flip_pixel (self, sample);
			if (sample % (self->priv->px_per_sec * self->priv->secondary_modulo) == 0) {
				cairo_move_to (cr, i, height);
				cairo_line_to (cr, i, height + 4);
				cairo_stroke (cr);
			}
		}
	}

	/* Primary marks and time strings */
	for (i = visible_first; i <= visible_last; i += 1) {
		sample = i;
		if (self->priv->rtl)
			sample = flip_pixel (self, sample);
		if (sample % (self->priv->px_per_sec * self->priv->primary_modulo) == 0) {
			gdk_cairo_set_source_rgba (cr, &self->priv->mark_color);
			cairo_move_to (cr, i, height);
			cairo_line_to (cr, i, height + 8);
			cairo_stroke (cr);
			gdk_cairo_set_source_rgba (cr, &self->priv->wave_color);
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

	gint pixel;
	gint array;
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
	for (pixel = visible_first; pixel <= visible_last; pixel += 1) {
		array = pixel_to_array (self, pixel);
		min = (middle + half * peaks[array] * -1);
		max = (middle - half * peaks[array + 1]);
		cairo_move_to (cr, pixel, min);
		cairo_line_to (cr, pixel, max);
		/* cairo_stroke also possible after loop, but then slower */
		cairo_stroke (cr);
	}

	/* paint selection */
	if (self->priv->sel_start != self->priv->sel_end) {
		gint start = time_to_pixel (self, self->priv->sel_start);
		gint end = time_to_pixel (self, self->priv->sel_end) - start;
		GdkRGBA sel_color;
		gdk_rgba_parse (&sel_color, "rgba(90%,50%,50%,0.5)");
		gdk_cairo_set_source_rgba (cr, &sel_color);
		cairo_rectangle (cr, start, 0, end, height);
		cairo_fill (cr);
	}

	/* paint ruler */
	if (self->priv->show_ruler)
		paint_ruler (self, cr, height, visible_first, visible_last);


	/* paint cursor */
	cairo_set_source_surface (cr,
	                          self->priv->cursor,
	                          time_to_pixel (self, self->priv->playback_cursor) - MARKER_BOX_W / 2,
	                          0);
	cairo_paint (cr);

	/* render focus */
	if (gtk_widget_has_focus (GTK_WIDGET (self->priv->drawarea))) {
		if (self->priv->focus_on_cursor) {
			gtk_render_focus (gtk_widget_get_style_context (GTK_WIDGET (self->priv->drawarea)),
					  cr,
					  time_to_pixel (self, self->priv->playback_cursor) - MARKER_BOX_W / 2 - 2,
					  1,
					  MARKER_BOX_W + 4,
					  height - 2);
		} else {
			gtk_render_focus (gtk_widget_get_style_context (GTK_WIDGET (self->priv->drawarea)),
					  cr,
					  visible_first + 1,
					  1,
					  (gint) gtk_adjustment_get_page_size (self->priv->adj) - 2,
					  gtk_widget_get_allocated_height (widget) - 2);
		}
	}

	return FALSE;
}

static gint64
add_subtract_time (PtWaveslider *self,
		   gint          pixel)
{
	/* add time to the cursor's time so that the cursor is moved x pixels */

	gint64 result;
	gint   one_pixel;

	one_pixel = 1000 / self->priv->px_per_sec;
	if (self->priv->rtl)
		one_pixel = one_pixel * -1;
	result = self->priv->playback_cursor + pixel * one_pixel;
	if (result < 0)
		result = 0;

	return result;
}

static gboolean
key_press_event_cb (GtkWidget   *widget,
		    GdkEventKey *event,
		    gpointer     data)
{
	PtWaveslider *slider = PT_WAVESLIDER (data);
	gdouble step;
	gdouble page;
	gdouble value;
	gdouble upper;

	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	if (!slider->priv->peaks)
		return FALSE;

	/* only Control is pressed, not together with Shift or Alt */
	if ((event->state & ALL_ACCELS_MASK) == GDK_CONTROL_MASK) {

		switch (event->keyval) {
		case GDK_KEY_Left:
		case GDK_KEY_Right:
		case GDK_KEY_Page_Up:
		case GDK_KEY_Page_Down:
		case GDK_KEY_Home:
		case GDK_KEY_End:
			/* override default scroll bindings  */
			return TRUE;
		}
	}

	if (slider->priv->focus_on_cursor) {

		/* no modifier pressed */
		if (!(event->state & ALL_ACCELS_MASK)) {

			switch (event->keyval) {
			case GDK_KEY_Left:
				g_signal_emit_by_name (slider, "cursor-changed", add_subtract_time (slider, -2));
				return TRUE;
			case GDK_KEY_Right:
				g_signal_emit_by_name (slider, "cursor-changed", add_subtract_time (slider, 2));
				return TRUE;
			case GDK_KEY_Page_Up:
				g_signal_emit_by_name (slider, "cursor-changed", add_subtract_time (slider, -20));
				return TRUE;
			case GDK_KEY_Page_Down:
				g_signal_emit_by_name (slider, "cursor-changed", add_subtract_time (slider, 20));
				return TRUE;
			case GDK_KEY_Home:
				g_signal_emit_by_name (slider, "cursor-changed", 0);
				return TRUE;
			case GDK_KEY_End:
				/* array size / 2 = samples; samples / pix per sec = seconds / * 1000 / + rounding errors */
				g_signal_emit_by_name (slider, "cursor-changed", slider->priv->peaks_size * 500 / slider->priv->px_per_sec + 10);
				return TRUE;
			}
		}
	} else {

		/* no modifier pressed */
		if (!(event->state & ALL_ACCELS_MASK)) {

			step = gtk_adjustment_get_step_increment (slider->priv->adj);
			page = gtk_adjustment_get_page_increment (slider->priv->adj);
			value = gtk_adjustment_get_value (slider->priv->adj);
			upper = gtk_adjustment_get_upper (slider->priv->adj);

			/* We scroll ourselves because we want to do it without modifier,
			   however it's not as smooth as the "real" scrolling */
			switch (event->keyval) {
			case GDK_KEY_Left:
				gtk_adjustment_set_value (slider->priv->adj, value - step);
				pt_waveslider_set_follow_cursor (slider, FALSE);
				return TRUE;
			case GDK_KEY_Right:
				gtk_adjustment_set_value (slider->priv->adj, value + step);
				pt_waveslider_set_follow_cursor (slider, FALSE);
				return TRUE;
			case GDK_KEY_Page_Up:
				gtk_adjustment_set_value (slider->priv->adj, value - page);
				pt_waveslider_set_follow_cursor (slider, FALSE);
				return TRUE;
			case GDK_KEY_Page_Down:
				gtk_adjustment_set_value (slider->priv->adj, value + page);
				pt_waveslider_set_follow_cursor (slider, FALSE);
				return TRUE;
			case GDK_KEY_Home:
				if (slider->priv->rtl)
					gtk_adjustment_set_value (slider->priv->adj, upper);
				else
					gtk_adjustment_set_value (slider->priv->adj, 0);
				pt_waveslider_set_follow_cursor (slider, FALSE);
				return TRUE;
			case GDK_KEY_End:
				if (slider->priv->rtl)
					gtk_adjustment_set_value (slider->priv->adj, 0);
				else
					gtk_adjustment_set_value (slider->priv->adj, upper);
				pt_waveslider_set_follow_cursor (slider, FALSE);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static void
set_selection (PtWaveslider *slider)
{
	if (slider->priv->dragstart == slider->priv->dragend) {
		slider->priv->sel_start = 0;
		slider->priv->sel_end = 0;
		return;
	}

	if (slider->priv->dragstart < slider->priv->dragend) {
		slider->priv->sel_start = slider->priv->dragstart;
		slider->priv->sel_end = slider->priv->dragend;
	} else {
		slider->priv->sel_start = slider->priv->dragend;
		slider->priv->sel_end = slider->priv->dragstart;
	}
}

static gboolean
pointer_in_range (PtWaveslider *slider,
		  gdouble       pointer,
		  gint64        pos)
{
	/* Returns TRUE if @pointer (event->x) is not more than 3 pixels away from @pos */

	return (fabs (pointer - (double) time_to_pixel (slider, pos)) < 3.0);
}

static void
set_cursor (GtkWidget *widget,
	    GdkCursor *cursor)
{
	GdkWindow  *gdkwin;
	gdkwin = gtk_widget_get_window (widget);
	gdk_window_set_cursor (gdkwin, cursor);
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
	pos = pixel_to_time (slider, clicked);

	slider->priv->dragstart = slider->priv->dragend = pos;
	/* Snap to selection border */
	if (pointer_in_range (slider, event->x, slider->priv->sel_start)) {
		slider->priv->dragstart = slider->priv->sel_end;
		slider->priv->dragend = slider->priv->sel_start;
	} else if (pointer_in_range (slider, event->x, slider->priv->sel_end)) {
		slider->priv->dragstart = slider->priv->sel_start;
		slider->priv->dragend = slider->priv->sel_end;
	}

	/* Single left click */
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY && !(event->state & ALL_ACCELS_MASK)) {
		set_cursor (widget, slider->priv->arrows);
		/* clear any previous selections */
		set_selection (slider);
		gtk_widget_queue_draw (GTK_WIDGET (slider->priv->drawarea));
		return TRUE;
	}

	/* Single left click with Shift pressed and existing selection */
	if (event->type == GDK_BUTTON_PRESS
	    && event->button == GDK_BUTTON_PRIMARY
	    && (event->state & ALL_ACCELS_MASK) == GDK_SHIFT_MASK
	    && slider->priv->sel_start != slider->priv->sel_end) {

		if (pos < slider->priv->sel_start)
			slider->priv->sel_start = pos;
		if (pos > slider->priv->sel_end)
			slider->priv->sel_end = pos;
		gtk_widget_queue_draw (GTK_WIDGET (slider->priv->drawarea));
		return TRUE;
	}

	/* Single right click */
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY) {
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
	pos = pixel_to_time (slider, clicked);

	if (event->state & GDK_BUTTON3_MASK) {
		g_signal_emit_by_name (slider, "cursor-changed", pos);
		return TRUE;
	}

	if (event->state & GDK_BUTTON1_MASK) {
		slider->priv->dragend = pos;
		set_selection (slider);
		gtk_widget_queue_draw (GTK_WIDGET (slider->priv->drawarea));
		return TRUE;
	}

	if (slider->priv->sel_start != slider->priv->sel_end) {
		if (pointer_in_range (slider, event->x, slider->priv->sel_start)
		    || pointer_in_range (slider, event->x, slider->priv->sel_end)) {
			set_cursor (widget, slider->priv->arrows);
		} else {
			set_cursor (widget, NULL);
		}
	}

	return FALSE;
}

static gboolean
button_release_event_cb (GtkWidget      *widget,
		         GdkEventButton *event,
		         gpointer        data)
{
	set_cursor (widget, NULL);
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

static void
adj_cb (GtkAdjustment *adj,
	gpointer       data)
{
	g_debug ("adjustment changed");

	/* GtkScrolledWindow draws itself mostly automatically, but some
	   adjustment changes are not propagated for reasons I don't understand.
	   Probably we're doing some draws twice */
	PtWaveslider *self = PT_WAVESLIDER (data);
	gtk_widget_queue_draw (GTK_WIDGET (self->priv->drawarea));
}

static gboolean
focus_cb (GtkWidget        *widget,
          GtkDirectionType  direction,
          gpointer          data)
{
	PtWaveslider *self = PT_WAVESLIDER (data);

	/* Focus chain forward: no-focus -> focus-whole-widget -> focus-cursor -> no-focus */
	if (gtk_widget_has_focus (widget)) {
		/* We have already focus, decide whether to stay in focus or not */
		if (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_DOWN) {
			if (self->priv->focus_on_cursor) {
				self->priv->focus_on_cursor = FALSE;
				/* focus lost */
			} else {
				self->priv->focus_on_cursor = TRUE;
				/* focus on cursor */
				gtk_widget_queue_draw (GTK_WIDGET (self->priv->drawarea));
				return TRUE;
			}
		}
		if (direction == GTK_DIR_TAB_BACKWARD || direction == GTK_DIR_UP) {
			if (self->priv->focus_on_cursor) {
				self->priv->focus_on_cursor = FALSE;
				/* focus on whole widget */
				gtk_widget_queue_draw (GTK_WIDGET (self->priv->drawarea));
				return TRUE;
			} else {
				/* focus lost */
			}
		}
	} else {
		/* We had no focus before, decide which part to focus on */
		if (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_DOWN || direction == GTK_DIR_RIGHT) {
			self->priv->focus_on_cursor = FALSE;
			/* focus on whole widget */
		} else {
			self->priv->focus_on_cursor = TRUE;
			/* focus on cursor */
		}
	}

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
	g_clear_object (&self->priv->arrows);

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
		/* ignore if cursor's value didn't change */
		if (self->priv->playback_cursor == g_value_get_int64 (value))
			break;
		gint64 old_value;
		old_value = self->priv->playback_cursor;
		self->priv->playback_cursor = g_value_get_int64 (value);

		/* ignore if we're not realized yet, widget is in construction */
		if (!gtk_widget_get_realized (GTK_WIDGET (self)))
			break;

		if (self->priv->follow_cursor)
			scroll_to_cursor (self);

		/* In fixed cursor mode we do an expensive redraw (this is recalculation)
		   of the whole visible area. There is plenty room for improvements here,
		   e.g. for some kind of caching.
		   In non-fixed cursor mode we invalidate the region where the cursor was
		   before and were it will be next. */

		if (self->priv->fixed_cursor) {
			gtk_widget_queue_draw (self->priv->drawarea);
		} else {
			gint height = gtk_widget_get_allocated_height (self->priv->drawarea);
			gint old_x = time_to_pixel (self, old_value) - MARKER_BOX_W / 2;
			gint new_x = time_to_pixel (self, self->priv->playback_cursor) - MARKER_BOX_W / 2;

			if (self->priv->show_ruler)
				height = height - self->priv->ruler_height;

			gtk_widget_queue_draw_area (self->priv->drawarea,
						    old_x,
						    0,
						    MARKER_BOX_W,
						    height);
			gtk_widget_queue_draw_area (self->priv->drawarea,
						    new_x,
						    0,
						    MARKER_BOX_W,
						    height);
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
		draw_cursor (self);
		gtk_widget_queue_draw (self->priv->drawarea);
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
	self->priv->focus_on_cursor = FALSE;
	self->priv->sel_start = 0;
	self->priv->sel_end = 0;
	self->priv->arrows = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);

	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
		self->priv->rtl = TRUE;
	else
		self->priv->rtl = FALSE;

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

	g_signal_connect (self->priv->drawarea,
			  "button-press-event",
			  G_CALLBACK (button_press_event_cb),
			  self);

	g_signal_connect (self->priv->drawarea,
			  "button-release-event",
			  G_CALLBACK (button_release_event_cb),
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

	g_signal_connect (self->priv->drawarea,
			  "focus",
			  G_CALLBACK (focus_cb),
			  self);

	g_signal_connect (self->priv->drawarea,
			  "key-press-event",
			  G_CALLBACK (key_press_event_cb),
			  self);

	gtk_widget_set_events (self->priv->drawarea, gtk_widget_get_events (self->priv->drawarea)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_BUTTON_RELEASE_MASK
				     | GDK_POINTER_MOTION_MASK
				     | GDK_KEY_PRESS_MASK);
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
			_("Cursor position"),
			_("Cursor's position in 1/1000 seconds"),
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
			_("Follow cursor"),
			_("Scroll automatically to the cursor's position"),
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

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
			_("Fixed cursor"),
			_("If TRUE, the cursor is in a fixed position and the waveform is moving.\n"
			"If FALSE, the cursor is moving."),
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	* PtWaveslider:show-ruler:
	*
	* Whether the ruler is shown (TRUE) or not (FALSE).
	*/

	obj_properties[PROP_SHOW_RULER] =
	g_param_spec_boolean (
			"show-ruler",
			_("Show ruler"),
			_("Show the time scale with time marks"),
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

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
