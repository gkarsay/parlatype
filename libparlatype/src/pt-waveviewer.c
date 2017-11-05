/* Copyright (C) 2006-2008 Buzztrax team <buzztrax-devel@buzztrax.org>
 * Copyright (C) 2002 2003 2004 2005 2007 2008 2009 2010, Magnus Hjorth
 * Copyright (C) 2016 Gabor Karsay <gabor.karsay@gmx.at>
 *
 * Original source name waveform-viewer.c, taken from Buzztrax and heavily
 * modified. Original source licenced under LGPL 2 or later.
 *
 * Small bits of selection stuff taken from mhWaveEdit 1.4.23 (Magnus Hjorth)
 * in files src/chunkview.c and src/document.c. Original source licenced under
 * GPL 2 or later.
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
#include "pt-ruler.h"
#include "pt-waveviewer-cursor.h"
#include "pt-waveviewer-focus.h"
#include "pt-waveviewer.h"


#define ALL_ACCELS_MASK	(GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)
#define CURSOR_POSITION 0.5
#define WAVE_MIN_HEIGHT 20

struct _PtWaveviewerPrivate {
	/* Wavedata */
	gfloat	 *peaks;
	gint64	  peaks_size;	/* size of array */
	gint	  px_per_sec;
	gint64    duration;	/* in milliseconds */

	/* Properties */
	gint64	  playback_cursor;
	gboolean  follow_cursor;
	gboolean  fixed_cursor;
	gboolean  show_ruler;
	gboolean  has_selection;

	/* Selections, in milliseconds */
	gint64    sel_start;
	gint64    sel_end;
	gint64    dragstart;
	gint64    dragend;
	GdkCursor *arrows;

	/* Drawing area */
	GtkWidget       *waveform;
	GtkAdjustment   *adj;
	gboolean         rtl;
	gboolean         focus_on_cursor;

	/* Subwidgets */
	GtkWidget  *ruler;
	GtkWidget  *waveviewer_focus;
	GtkWidget  *waveviewer_cursor;

	/* Colors */
	GdkRGBA	  wave_color;
	GdkRGBA	  selection_color;
};

enum
{
	PROP_0,
	PROP_PLAYBACK_CURSOR,
	PROP_FOLLOW_CURSOR,
	PROP_FIXED_CURSOR,
	PROP_SHOW_RULER,
	PROP_HAS_SELECTION,
	PROP_SELECTION_START,
	PROP_SELECTION_END,
	N_PROPERTIES
};

enum {
	CURSOR_CHANGED,
	SELECTION_CHANGED,
	PLAY_TOGGLED,
	LAST_SIGNAL
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewer, pt_waveviewer, GTK_TYPE_SCROLLED_WINDOW);


/**
 * SECTION: pt-waveviewer
 * @short_description: A GtkWidget to display a waveform.
 * @stability: Unstable
 * @include: parlatype/pt-waveviewer.h
 *
 * Displays a waveform provided by #PtPlayer or #PtWaveloader.
 */


static gint64
flip_pixel (PtWaveviewer *self,
	    gint64        pixel)
{
	/* In ltr layouts each pixel on the x-axis corresponds to a sample in the array.
	   In rtl layouts this is flipped, e.g. the first pixel corresponds to
	   the last sample in the array. */

	gint64 samples;
	gint   widget_width;

	samples = self->priv->peaks_size / 2;
	widget_width = gtk_widget_get_allocated_width (self->priv->waveform);

	/* Case: waveform is shorter than drawing area */
	if (samples < widget_width)
		return (widget_width - pixel);
	else
		return (samples - pixel);
}

static gint64
time_to_pixel (PtWaveviewer *self,
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
pixel_to_time (PtWaveviewer *self,
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
scroll_to_cursor (PtWaveviewer *self)
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
	PtWaveviewer *self = (PtWaveviewer *) widget;
	/* If widget changed horizontal size, revealed parts have to be drawn */
	gtk_widget_queue_draw (self->priv->waveform);
}

static gint64
pixel_to_array (PtWaveviewer *self,
		gint64	      pixel)
{
	/* Convert a position in the drawing area to an index in the peaks array.
	   The returned index is the peak's min value, +1 is the max value.
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
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
	/* Redraw screen, cairo_t is already clipped to only draw the exposed
	   areas of the drawing area */

	g_debug ("draw_cb");

	PtWaveviewer *self = (PtWaveviewer *) data;

	gfloat *peaks = self->priv->peaks;
	GtkStyleContext *context;
	gint visible_first;
	gint visible_last;
	gint pixel;
	gint array;
	gdouble min, max;

	context = gtk_widget_get_style_context (self->priv->waveform);
	gint height = gtk_widget_get_allocated_height (widget);

	gint half = height / 2 - 1;
	gint middle = height / 2;

	visible_first = (gint) gtk_adjustment_get_value (self->priv->adj);
	visible_last = visible_first + (gint) gtk_adjustment_get_page_size (self->priv->adj);

	g_debug ("visible area: %d–%d", visible_first, visible_last);

	gtk_render_background (context, cr,
                               visible_first, 0,
                               visible_last - visible_first, height);
	if (!peaks)
		return FALSE;

	/* paint waveform */
	gdk_cairo_set_source_rgba (cr, &self->priv->wave_color);
	for (pixel = visible_first; pixel <= visible_last; pixel += 1) {
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

	/* paint selection */
	if (self->priv->sel_start != self->priv->sel_end) {
		gint start = time_to_pixel (self, self->priv->sel_start);
		gint end = time_to_pixel (self, self->priv->sel_end) - start;
		gdk_cairo_set_source_rgba (cr, &self->priv->selection_color);
		cairo_rectangle (cr, start, 0, end, height);
		cairo_fill (cr);
	}

	return FALSE;
}

static gint64
calculate_duration (PtWaveviewer *self)
{
	gint64 result;
	gint64 samples;
	gint one_pixel;

	one_pixel = 1000 / self->priv->px_per_sec;
	samples = self->priv->peaks_size / 2;
	result = samples / self->priv->px_per_sec * 1000; /* = full seconds */
	result += (samples % self->priv->px_per_sec) * one_pixel;

	return result;
}

static gint64
add_subtract_time (PtWaveviewer *self,
		   gint          pixel,
		   gboolean      stay_in_selection)
{
	/* add time to the cursor's time so that the cursor is moved x pixels */

	gint64 result;
	gint   one_pixel;

	one_pixel = 1000 / self->priv->px_per_sec;
	if (self->priv->rtl)
		one_pixel = one_pixel * -1;
	result = self->priv->playback_cursor + pixel * one_pixel;

	/* Return result in range */
	if (stay_in_selection)
		return CLAMP (result, self->priv->sel_start, self->priv->sel_end);
	else
		return CLAMP (result, 0, self->priv->duration);
}

static void
update_selection (PtWaveviewer *self)
{
	/* Check if drag positions are different from selection.
	   If yes, set new selection, emit signals and redraw widget. */

	gboolean changed = FALSE;

	/* Is anything selected at all? */
	if (self->priv->dragstart == self->priv->dragend) {
		self->priv->sel_start = 0;
		self->priv->sel_end = 0;
		if (self->priv->has_selection) {
			self->priv->has_selection = FALSE;
			g_object_notify_by_pspec (G_OBJECT (self),
						  obj_properties[PROP_HAS_SELECTION]);
			g_signal_emit_by_name (self, "selection-changed");
			gtk_widget_queue_draw (self->priv->waveform);
		}
		return;
	}

	/* Sort start/end, check for changes (no changes on vertical movement), update selection */
	if (self->priv->dragstart < self->priv->dragend) {
		if (self->priv->sel_start != self->priv->dragstart || self->priv->sel_end != self->priv->dragend) {
			self->priv->sel_start = self->priv->dragstart;
			self->priv->sel_end = self->priv->dragend;
			changed = TRUE;
		}
	} else {
		if (self->priv->sel_start != self->priv->dragend || self->priv->sel_end != self->priv->dragstart) {
			self->priv->sel_start = self->priv->dragend;
			self->priv->sel_end = self->priv->dragstart;
			changed = TRUE;
		}
	}

	if (changed) {

		/* Update has-selection property */
		if (!self->priv->has_selection) {
			self->priv->has_selection = TRUE;
			g_object_notify_by_pspec (G_OBJECT (self),
						  obj_properties[PROP_HAS_SELECTION]);
		}

		g_signal_emit_by_name (self, "selection-changed");
		gtk_widget_queue_draw (self->priv->waveform);
	}
}

static gboolean
key_press_event_cb (GtkWidget   *widget,
		    GdkEventKey *event,
		    gpointer     data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (widget);
	gdouble step;
	gdouble page;
	gdouble value;
	gdouble upper;

	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	if (!self->priv->peaks)
		return FALSE;

	/* no modifier pressed */
	if (!(event->state & ALL_ACCELS_MASK)) {
		switch (event->keyval) {
		case GDK_KEY_Escape:
			self->priv->dragstart = self->priv->dragend = 0;
			update_selection (self);
			return TRUE;
		case GDK_KEY_space:
			g_signal_emit_by_name (self, "play-toggled");
			return TRUE;
		}
	}

	if (self->priv->focus_on_cursor) {

		/* no modifier pressed */
		if (!(event->state & ALL_ACCELS_MASK)) {

			switch (event->keyval) {
			case GDK_KEY_Left:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, -2, FALSE));
				return TRUE;
			case GDK_KEY_Right:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, 2, FALSE));
				return TRUE;
			case GDK_KEY_Page_Up:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, -20, FALSE));
				return TRUE;
			case GDK_KEY_Page_Down:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, 20, FALSE));
				return TRUE;
			case GDK_KEY_Home:
				g_signal_emit_by_name (self, "cursor-changed", 0);
				return TRUE;
			case GDK_KEY_End:
				g_signal_emit_by_name (self, "cursor-changed", self->priv->duration);
				return TRUE;
			}
		}
		/* Only Control is pressed, not together with Shift or Alt
		   Here we override the default horizontal scroll bindings */
		else if ((event->state & ALL_ACCELS_MASK) == GDK_CONTROL_MASK) {

			switch (event->keyval) {
			case GDK_KEY_Left:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, -2, TRUE));
				return TRUE;
			case GDK_KEY_Right:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, 2, TRUE));
				return TRUE;
			case GDK_KEY_Page_Up:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, -20, TRUE));
				return TRUE;
			case GDK_KEY_Page_Down:
				g_signal_emit_by_name (self, "cursor-changed", add_subtract_time (self, 20, TRUE));
				return TRUE;
			case GDK_KEY_Home:
				g_signal_emit_by_name (self, "cursor-changed", self->priv->sel_start);
				return TRUE;
			case GDK_KEY_End:
				g_signal_emit_by_name (self, "cursor-changed", self->priv->sel_end);
				return TRUE;
			}
		}
	} else {

		/* no modifier pressed */
		if (!(event->state & ALL_ACCELS_MASK)) {

			step = gtk_adjustment_get_step_increment (self->priv->adj);
			page = gtk_adjustment_get_page_increment (self->priv->adj);
			value = gtk_adjustment_get_value (self->priv->adj);
			upper = gtk_adjustment_get_upper (self->priv->adj);

			/* We scroll ourselves because we want to do it without modifier,
			   however it's not as smooth as the "real" scrolling */
			switch (event->keyval) {
			case GDK_KEY_Left:
				gtk_adjustment_set_value (self->priv->adj, value - step);
				pt_waveviewer_set_follow_cursor (self, FALSE);
				return TRUE;
			case GDK_KEY_Right:
				gtk_adjustment_set_value (self->priv->adj, value + step);
				pt_waveviewer_set_follow_cursor (self, FALSE);
				return TRUE;
			case GDK_KEY_Page_Up:
				gtk_adjustment_set_value (self->priv->adj, value - page);
				pt_waveviewer_set_follow_cursor (self, FALSE);
				return TRUE;
			case GDK_KEY_Page_Down:
				gtk_adjustment_set_value (self->priv->adj, value + page);
				pt_waveviewer_set_follow_cursor (self, FALSE);
				return TRUE;
			case GDK_KEY_Home:
				if (self->priv->rtl)
					gtk_adjustment_set_value (self->priv->adj, upper);
				else
					gtk_adjustment_set_value (self->priv->adj, 0);
				pt_waveviewer_set_follow_cursor (self, FALSE);
				return TRUE;
			case GDK_KEY_End:
				if (self->priv->rtl)
					gtk_adjustment_set_value (self->priv->adj, 0);
				else
					gtk_adjustment_set_value (self->priv->adj, upper);
				pt_waveviewer_set_follow_cursor (self, FALSE);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
pointer_in_range (PtWaveviewer *self,
		  gdouble       pointer,
		  gint64        pos)
{
	/* Returns TRUE if @pointer (event->x) is not more than 3 pixels away from @pos */

	return (fabs (pointer - (double) time_to_pixel (self, pos)) < 3.0);
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
	PtWaveviewer *self = PT_WAVEVIEWER (widget);

	if (!self->priv->peaks)
		return FALSE;

	gint64 clicked;	/* the sample clicked on */
	gint64 pos;	/* clicked sample's position in milliseconds */

	clicked = (gint) event->x;
	pos = pixel_to_time (self, clicked);


	/* Single left click, no other keys pressed: new selection or changing selection */
	if (event->type == GDK_BUTTON_PRESS
	    && event->button == GDK_BUTTON_PRIMARY
	    && !(event->state & ALL_ACCELS_MASK)) {
		/* set position as start and end point, for new selection */
		self->priv->dragstart = self->priv->dragend = pos;

		/* if over selection border: snap to selection border, changing selection */
		if (pointer_in_range (self, event->x, self->priv->sel_start)) {
			self->priv->dragstart = self->priv->sel_end;
			self->priv->dragend = self->priv->sel_start;
		} else if (pointer_in_range (self, event->x, self->priv->sel_end)) {
			self->priv->dragstart = self->priv->sel_start;
			self->priv->dragend = self->priv->sel_end;
		}

		set_cursor (widget, self->priv->arrows);
		update_selection (self);
		return TRUE;
	}

	/* Single left click with Shift pressed and existing selection: enlarge selection */
	if (event->type == GDK_BUTTON_PRESS
	    && event->button == GDK_BUTTON_PRIMARY
	    && (event->state & ALL_ACCELS_MASK) == GDK_SHIFT_MASK
	    && self->priv->sel_start != self->priv->sel_end) {

		self->priv->dragend = pos;

		if (pos < self->priv->sel_start)
			self->priv->dragstart = self->priv->sel_end;
		else
			self->priv->dragstart = self->priv->sel_start;

		set_cursor (widget, self->priv->arrows);
		update_selection (self);
		return TRUE;
	}

	/* Single right click: change cursor */
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY) {
		g_signal_emit_by_name (self, "cursor-changed", pos);
		return TRUE;
	}

	return FALSE;
}

static gboolean
motion_notify_event_cb (GtkWidget      *widget,
                        GdkEventMotion *event,
                        gpointer        data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (widget);

	if (!self->priv->peaks)
		return FALSE;

	gint64 clicked;	/* the sample clicked on */
	gint64 pos;	/* clicked sample's position in milliseconds */

	clicked = (gint) event->x;
	pos = pixel_to_time (self, clicked);

	/* Right mouse button sets cursor */
	if (event->state & GDK_BUTTON3_MASK) {
		g_signal_emit_by_name (self, "cursor-changed", pos);
		return TRUE;
	}

	/* Left mouse button (with or without Shift key) sets selection */
	if (event->state & GDK_BUTTON1_MASK || event->state & GDK_BUTTON1_MASK & GDK_SHIFT_MASK) {
		self->priv->dragend = pos;
		update_selection (self);
		return TRUE;
	}

	/* No button or any other button: change pointer cursor over selection border */
	if (self->priv->sel_start != self->priv->sel_end) {
		if (pointer_in_range (self, event->x, self->priv->sel_start)
		    || pointer_in_range (self, event->x, self->priv->sel_end)) {
			set_cursor (widget, self->priv->arrows);
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
pt_waveviewer_update_cached_style_values (PtWaveviewer *self)
{
	/* Colors and direction are cached */

	GtkStyleContext *context;
	GtkStateFlags    state;
	GdkWindow       *window = NULL;

	window = gtk_widget_get_parent_window (GTK_WIDGET (self));
	if (!window)
		return;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	state = gtk_style_context_get_state (context);
	gtk_style_context_save (context);

	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
	gtk_style_context_get_color (context, state, &self->priv->wave_color);

	gtk_style_context_add_class (context, "selection");
	gtk_style_context_get_color (context, state, &self->priv->selection_color);

	if (state & GTK_STATE_FLAG_DIR_RTL)
		self->priv->rtl = TRUE;
	else
		self->priv->rtl = FALSE;

	gtk_style_context_restore (context);
}

static void
pt_waveviewer_state_flags_changed (GtkWidget	 *widget,
				   GtkStateFlags  flags)
{
	/* Update colors */

	PtWaveviewer *self = PT_WAVEVIEWER (widget);
	pt_waveviewer_update_cached_style_values (self);

	GTK_WIDGET_CLASS (pt_waveviewer_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_waveviewer_style_updated (GtkWidget *widget)
{
	PtWaveviewer *self = PT_WAVEVIEWER (widget);

	GTK_WIDGET_CLASS (pt_waveviewer_parent_class)->style_updated (widget);

	pt_waveviewer_update_cached_style_values (self);
	gtk_widget_queue_draw (self->priv->waveform);
}

static gboolean
scrollbar_cb (GtkWidget      *widget,
	      GdkEventButton *event,
	      gpointer        data)
{
	/* If user clicks on scrollbar don't follow cursor anymore.
	   Otherwise it would scroll immediately back again. */

	PtWaveviewer *self = PT_WAVEVIEWER (data);
	pt_waveviewer_set_follow_cursor (self, FALSE);

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
	PtWaveviewer *self = PT_WAVEVIEWER (data);
	gtk_widget_queue_draw (self->priv->waveform);
}

static void
focus_cursor (PtWaveviewer *self)
{
	self->priv->focus_on_cursor = TRUE;
	pt_waveviewer_cursor_set_focus (PT_WAVEVIEWER_CURSOR  (self->priv->waveviewer_cursor), TRUE);
	pt_waveviewer_focus_set (PT_WAVEVIEWER_FOCUS  (self->priv->waveviewer_focus), FALSE);
}

static void
focus_widget (PtWaveviewer *self)
{
	self->priv->focus_on_cursor = FALSE;
	pt_waveviewer_cursor_set_focus (PT_WAVEVIEWER_CURSOR  (self->priv->waveviewer_cursor), FALSE);
	pt_waveviewer_focus_set (PT_WAVEVIEWER_FOCUS  (self->priv->waveviewer_focus), TRUE);
}

static void
focus_lost (PtWaveviewer *self)
{
	self->priv->focus_on_cursor = FALSE;
	pt_waveviewer_cursor_set_focus (PT_WAVEVIEWER_CURSOR  (self->priv->waveviewer_cursor), FALSE);
	pt_waveviewer_focus_set (PT_WAVEVIEWER_FOCUS  (self->priv->waveviewer_focus), FALSE);
}

static gboolean
focus_out_cb (GtkWidget *widget,
              GdkEvent  *event,
              gpointer   data)
{
	focus_lost (PT_WAVEVIEWER (widget));
	return FALSE;
}

static gboolean
focus_in_cb (GtkWidget *widget,
             GdkEvent  *event,
             gpointer   data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (widget);
	if (!self->priv->focus_on_cursor && self->priv->peaks)
		focus_widget (self);
	return FALSE;
}

static gboolean
focus_cb (GtkWidget        *widget,
          GtkDirectionType  direction,
          gpointer          data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (widget);

	/* See API reference for gtk_widget_child_focus ():
	   If moving into @direction would move focus outside of widget: return FALSE;
	   If moving into @direction would stay inside widget: return TRUE */

	/* Don't focus if empty */
	if (!self->priv->peaks)
		return FALSE;

	/* Focus chain forward: no-focus -> focus-whole-widget -> focus-cursor -> no-focus */
	if (gtk_widget_has_focus (widget)) {
		/* We have already focus, decide whether to stay in focus or not */
		if (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_DOWN) {
			if (self->priv->focus_on_cursor) {
				focus_lost (self);
			} else {
				focus_cursor (self);
				return TRUE;
			}
		}
		if (direction == GTK_DIR_TAB_BACKWARD || direction == GTK_DIR_UP) {
			if (self->priv->focus_on_cursor) {
				focus_widget (self);
				return TRUE;
			} else {
				focus_lost (self);
			}
		}
	} else {
		/* We had no focus before, decide which part to focus on */
		if (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_DOWN || direction == GTK_DIR_RIGHT) {
			focus_widget (self);
		} else {
			focus_cursor (self);
		}
	}

	return FALSE;
}

/**
 * pt_waveviewer_get_follow_cursor:
 * @self: the widget
 *
 * Get follow-cursor option.
 *
 * Return value: TRUE if cursor is followed, else FALSE
 */
gboolean
pt_waveviewer_get_follow_cursor (PtWaveviewer *self)
{
	g_return_val_if_fail (PT_IS_WAVEVIEWER (self), FALSE);

	return self->priv->follow_cursor;
}

/**
 * pt_waveviewer_set_follow_cursor:
 * @self: the widget
 * @follow: new value
 *
 * Set follow-cursor option to TRUE or FALSE. See also #PtWaveviewer:follow-cursor.
 */
void
pt_waveviewer_set_follow_cursor (PtWaveviewer *self,
				 gboolean      follow)
{
	g_return_if_fail (PT_IS_WAVEVIEWER (self));

	if (self->priv->follow_cursor != follow) {
		self->priv->follow_cursor = follow;
		g_object_notify_by_pspec (G_OBJECT (self),
					  obj_properties[PROP_FOLLOW_CURSOR]);
		if (follow)
			scroll_to_cursor (self);
	}
}

/**
 * pt_waveviewer_set_wave:
 * @self: the widget
 * @data: (allow-none): a #PtWavedata
 *
 * Set wave data to show in the widget. The data is copied internally and may
 * be freed immediately after calling this function. If @data is NULL, a blank
 * widget is drawn.
 */
void
pt_waveviewer_set_wave (PtWaveviewer *self,
			PtWavedata   *data)
{
	g_return_if_fail (PT_IS_WAVEVIEWER (self));

	g_debug ("set_wave");

	if (self->priv->peaks) {
		g_free (self->priv->peaks);
		self->priv->peaks = NULL;
	}

	self->priv->duration = 0;
	self->priv->playback_cursor = 0;
	gtk_widget_set_size_request (self->priv->waveform, -1, WAVE_MIN_HEIGHT);

	if (!data) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		pt_ruler_set_ruler (PT_RULER (self->priv->ruler), 0, 0, 0);
		return;
	}

	if (!data->array || !data->length) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		pt_ruler_set_ruler (PT_RULER (self->priv->ruler), 0, 0, 0);
		return;
	}

	/* Copy array. If wiget is not owner of its data, bad things can happen
	   with bindings. */
	if (!(self->priv->peaks = g_malloc (sizeof (gfloat) * data->length))) {
		g_debug	("waveviewer failed to allocate memory");
		gtk_widget_queue_draw (GTK_WIDGET (self));
		pt_ruler_set_ruler (PT_RULER (self->priv->ruler), 0, 0, 0);
		return;
	}

	memcpy (self->priv->peaks, data->array, sizeof(gfloat) * data->length);
	self->priv->peaks_size = data->length;
	self->priv->px_per_sec = data->px_per_sec;
	self->priv->duration = calculate_duration (self);

	/* Reset previous selections */
	self->priv->sel_start = self->priv->sel_end = 0;
	self->priv->has_selection = FALSE;
	g_object_notify_by_pspec (G_OBJECT (self),
				  obj_properties[PROP_SELECTION_START]);
	g_object_notify_by_pspec (G_OBJECT (self),
				  obj_properties[PROP_SELECTION_END]);
	g_object_notify_by_pspec (G_OBJECT (self),
				  obj_properties[PROP_HAS_SELECTION]);

	pt_ruler_set_ruler (PT_RULER (self->priv->ruler),
			    self->priv->peaks_size / 2,
			    self->priv->px_per_sec,
			    self->priv->duration);

	gtk_widget_set_size_request (self->priv->waveform, data->length / 2, WAVE_MIN_HEIGHT);
	gtk_widget_set_size_request (self->priv->waveviewer_cursor, data->length / 2, WAVE_MIN_HEIGHT);
	gtk_adjustment_set_upper (self->priv->adj, data->length / 2);

	/* It seems like the state_flags_changed and style_updated flags are
	   never emitted under KDE/Plasma desktop or only once at the beginning
	   when there is no parent window yet. If we don't manually update the
	   cached style values, the colors are not set at all under KDE/Plasma. */
	pt_waveviewer_update_cached_style_values (self);

	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_finalize (GObject *object)
{
	PtWaveviewer *self = PT_WAVEVIEWER (object);

	g_free (self->priv->peaks);
	g_clear_object (&self->priv->arrows);

	G_OBJECT_CLASS (pt_waveviewer_parent_class)->finalize (object);
}

static void
pt_waveviewer_get_property (GObject    *object,
			    guint       property_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	PtWaveviewer *self = PT_WAVEVIEWER (object);

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
	case PROP_HAS_SELECTION:
		g_value_set_boolean (value, self->priv->has_selection);
		break;
	case PROP_SELECTION_START:
		g_value_set_int64 (value, self->priv->sel_start);
		break;
	case PROP_SELECTION_END:
		g_value_set_int64 (value, self->priv->sel_end);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveviewer_set_property (GObject      *object,
			    guint	  property_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	PtWaveviewer *self = PT_WAVEVIEWER (object);

	switch (property_id) {
	case PROP_PLAYBACK_CURSOR:
		/* ignore if cursor's value didn't change */
		if (self->priv->playback_cursor == g_value_get_int64 (value))
			break;
		self->priv->playback_cursor = g_value_get_int64 (value);

		/* ignore if we're not realized yet, widget is in construction */
		if (!gtk_widget_get_realized (GTK_WIDGET (self)))
			break;

		if (self->priv->follow_cursor)
			scroll_to_cursor (self);

		pt_waveviewer_cursor_render (PT_WAVEVIEWER_CURSOR (self->priv->waveviewer_cursor),
					     time_to_pixel (self, self->priv->playback_cursor));
		break;
	case PROP_FOLLOW_CURSOR:
		self->priv->follow_cursor = g_value_get_boolean (value);
		if (gtk_widget_get_realized (GTK_WIDGET (self)) && self->priv->follow_cursor)
			scroll_to_cursor (self);
		break;
	case PROP_FIXED_CURSOR:
		self->priv->fixed_cursor = g_value_get_boolean (value);
		break;
	case PROP_SHOW_RULER:
		self->priv->show_ruler = g_value_get_boolean (value);
		if (self->priv->show_ruler) {
			gtk_widget_show (self->priv->ruler);
		} else {
			gtk_widget_hide (self->priv->ruler);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveviewer_constructed (GObject *object)
{
	g_debug ("waveviewer constructed");

	PtWaveviewer *self = PT_WAVEVIEWER (object);
	self->priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (self));
	g_signal_connect (self->priv->adj, "value-changed", G_CALLBACK (adj_cb), self);

	g_signal_connect (gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (self)),
			  "button_press_event",
			  G_CALLBACK (scrollbar_cb),
			  self);
}

static void
pt_waveviewer_init (PtWaveviewer *self)
{
	self->priv = pt_waveviewer_get_instance_private (self);

	GtkWidget       *box, *overlay;
	GdkDisplay      *display;
	GtkStyleContext *context;
	GtkCssProvider  *provider;
	GFile		*css_file;

	self->priv->peaks = NULL;
	self->priv->peaks_size = 0;
	self->priv->duration = 0;
	self->priv->playback_cursor = 0;
	self->priv->follow_cursor = TRUE;
	self->priv->focus_on_cursor = FALSE;
	self->priv->sel_start = 0;
	self->priv->sel_end = 0;

	/* Setup scrolled window */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	overlay = gtk_overlay_new ();
	self->priv->waveform = gtk_drawing_area_new ();
	self->priv->waveviewer_focus = pt_waveviewer_focus_new ();
	self->priv->waveviewer_cursor = pt_waveviewer_cursor_new ();
	self->priv->ruler = pt_ruler_new ();
	gtk_container_add (GTK_CONTAINER (overlay), self->priv->waveform);
	gtk_overlay_add_overlay (GTK_OVERLAY (overlay), self->priv->waveviewer_cursor);
	gtk_overlay_add_overlay (GTK_OVERLAY (overlay), self->priv->waveviewer_focus);
	gtk_container_add_with_properties (
			GTK_CONTAINER (box),
			overlay,
			"expand", TRUE,
			"fill", TRUE,
			NULL);
	gtk_container_add (GTK_CONTAINER (box), self->priv->ruler);
	gtk_container_add (GTK_CONTAINER (self), box);
	gtk_scrolled_window_set_policy (
			GTK_SCROLLED_WINDOW (self),
			GTK_POLICY_ALWAYS,
			GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type (
			GTK_SCROLLED_WINDOW (self),
			GTK_SHADOW_IN);
	gtk_widget_show_all (GTK_WIDGET (self));

	/* Try a few cursors for selections */
	display = gdk_display_get_default ();
	self->priv->arrows = gdk_cursor_new_from_name (display, "ew-resize");
	if (!self->priv->arrows)
		self->priv->arrows = gdk_cursor_new_from_name (display, "col-resize");
	if (!self->priv->arrows)
		self->priv->arrows = gdk_cursor_new_for_display (display, GDK_SB_H_DOUBLE_ARROW);

	css_file = g_file_new_for_uri ("resource:///com/github/gkarsay/libparlatype/pt-waveviewer.css");
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_file (provider, css_file, NULL);
	gtk_style_context_add_provider_for_screen (
			gdk_screen_get_default (),
			GTK_STYLE_PROVIDER (provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	context = gtk_widget_get_style_context (self->priv->waveform);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
	gtk_style_context_add_class (context, "selection");

	g_object_unref (css_file);
	g_object_unref (provider);

#if GTK_CHECK_VERSION(3,16,0)
	gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (self), FALSE);
#endif

	g_signal_connect (self,
			  "button-press-event",
			  G_CALLBACK (button_press_event_cb),
			  NULL);

	g_signal_connect (self,
			  "button-release-event",
			  G_CALLBACK (button_release_event_cb),
			  NULL);

	g_signal_connect (self->priv->waveform,
			  "draw",
			  G_CALLBACK (draw_cb),
			  self);

	g_signal_connect (self,
			  "motion-notify-event",
			  G_CALLBACK (motion_notify_event_cb),
			  NULL);

	g_signal_connect (self,
			  "size-allocate",
			  G_CALLBACK (size_allocate_cb),
			  NULL);

	g_signal_connect (self,
			  "focus",
			  G_CALLBACK (focus_cb),
			  NULL);

	g_signal_connect (self,
			  "focus-out-event",
			  G_CALLBACK (focus_out_cb),
			  NULL);

	g_signal_connect (self,
			  "focus-in-event",
			  G_CALLBACK (focus_in_cb),
			  NULL);

	g_signal_connect (self,
			  "key-press-event",
			  G_CALLBACK (key_press_event_cb),
			  NULL);

	gtk_widget_set_events (self->priv->waveform, gtk_widget_get_events (self->priv->waveform)
                                     | GDK_BUTTON_PRESS_MASK
                                     | GDK_BUTTON_RELEASE_MASK
				     | GDK_POINTER_MOTION_MASK
				     | GDK_KEY_PRESS_MASK);
}

static void
pt_waveviewer_class_init (PtWaveviewerClass *klass)
{
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = pt_waveviewer_set_property;
	gobject_class->get_property = pt_waveviewer_get_property;
	gobject_class->constructed  = pt_waveviewer_constructed;
	gobject_class->finalize     = pt_waveviewer_finalize;

	widget_class->state_flags_changed = pt_waveviewer_state_flags_changed;
	widget_class->style_updated       = pt_waveviewer_style_updated;


	/**
	* PtWaveviewer::cursor-changed:
	* @viewer: the waveviewer emitting the signal
	* @position: the new position in stream in milliseconds
	*
	* Signals that the cursor's position was changed by the user.
	*/
	signals[CURSOR_CHANGED] =
	g_signal_new ("cursor-changed",
		      PT_TYPE_WAVEVIEWER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__INT,
		      G_TYPE_NONE,
		      1, G_TYPE_INT64);

	/**
	* PtWaveviewer::selection-changed:
	* @viewer: the waveviewer emitting the signal
	*
	* Signals that the selection was changed (or unselected) by the user.
	* To query the new selection see #PtWaveviewer:has-selection,
	* #PtWaveviewer:selection-start and #PtWaveviewer:selection-end.
	*/
	signals[SELECTION_CHANGED] =
	g_signal_new ("selection-changed",
		      PT_TYPE_WAVEVIEWER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	/**
	* PtWaveviewer::play-toggled:
	* @viewer: the waveviewer emitting the signal
	*
	* Signals that the user requested to toggle play/pause.
	*/
	signals[PLAY_TOGGLED] =
	g_signal_new ("play-toggled",
		      PT_TYPE_WAVEVIEWER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	/**
	* PtWaveviewer:playback-cursor:
	*
	* Current playback position in milliseconds.
	*/

	obj_properties[PROP_PLAYBACK_CURSOR] =
	g_param_spec_int64 (
			"playback-cursor",
			"Cursor position",
			"Cursor's position in 1/1000 seconds",
			0,
			G_MAXINT64,
			0,
			G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

	/**
	* PtWaveviewer:follow-cursor:
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
	* PtWaveviewer:fixed-cursor:
	*
	* If TRUE, in follow-cursor mode the cursor is at a fixed position and the
	* waveform is scrolling. If FALSE the cursor is moving.
	*
	* If #PtWaveviewer:follow-cursor is FALSE, this has no effect.
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
	* PtWaveviewer:show-ruler:
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

	/**
	* PtWaveviewer:has-selection:
	*
	* Whether something is selected (TRUE) or not (FALSE).
	*/

	obj_properties[PROP_HAS_SELECTION] =
	g_param_spec_boolean (
			"has-selection",
			"Has selection",
			"Indicates whether something is selected",
			FALSE,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	* PtWaveviewer:selection-start:
	*
	* Start time of selection in milliseconds. If it's equal to the end
	* time, there is no selection. See also #PtWaveviewer:has-selection.
	*/

	obj_properties[PROP_SELECTION_START] =
	g_param_spec_int64 (
			"selection-start",
			"Start time of selection",
			"Start time of selection",
			0,
			G_MAXINT64,
			0,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	* PtWaveviewer:selection-end:
	*
	* End time of selection in milliseconds. If it's equal to the start
	* time, there is no selection. See also #PtWaveviewer:has-selection.
	*/

	obj_properties[PROP_SELECTION_END] =
	g_param_spec_int64 (
			"selection-end",
			"End time of selection",
			"End time of selection",
			0,
			G_MAXINT64,
			0,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (
			gobject_class,
			N_PROPERTIES,
			obj_properties);
}

/**
 * pt_waveviewer_new:
 *
 * Create a new, initially blank waveform viewer widget. Use
 * pt_waveviewer_set_wave() to pass wave data.
 *
 * After use gtk_widget_destroy() it.
 *
 * Returns: the widget
 */
GtkWidget *
pt_waveviewer_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER, NULL));
}
