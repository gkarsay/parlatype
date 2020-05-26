/* Copyright (C) 2006-2008 Buzztrax team <buzztrax-devel@buzztrax.org>
 * Copyright (C) 2002 2003 2004 2005 2007 2008 2009 2010, Magnus Hjorth
 * Copyright (C) 2016, 2020 Gabor Karsay <gabor.karsay@gmx.at>
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

/**
 * SECTION: pt-waveviewer
 * @short_description: A GtkWidget to display a waveform.
 * @stability: Stable
 * @include: parlatype/pt-waveviewer.h
 *
 * Displays a waveform and lets the user interact with it: jumping to a position,
 * make selections and so on.
 */


#include "config.h"
#include <math.h>	/* fabs */
#include <string.h>
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include "pt-marshalers.h"
#include "pt-waveloader.h"
#include "pt-waveviewer-ruler.h"
#include "pt-waveviewer-waveform.h"
#include "pt-waveviewer-scrollbox.h"
#include "pt-waveviewer-selection.h"
#include "pt-waveviewer-cursor.h"
#include "pt-waveviewer-focus.h"
#include "pt-waveviewer.h"


#define ALL_ACCELS_MASK	(GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)
#define CURSOR_POSITION 0.5
#define WAVE_MIN_HEIGHT 20

struct _PtWaveviewerPrivate {
	/* Wavedata */
	PtWaveloader *loader;
	GArray   *peaks;
	gint64    peaks_size;	/* size of array */
	gint      px_per_sec;
	gint64    duration;	/* in milliseconds */

	/* Properties */
	gint64    playback_cursor;
	gboolean  follow_cursor;
	gboolean  fixed_cursor;
	gboolean  show_ruler;
	gboolean  has_selection;
	gint      pps;

	gint64    zoom_time;
	gint      zoom_pos;

	/* Selections, in milliseconds */
	gint64    sel_start;
	gint64    sel_end;
	gint64    dragstart;
	gint64    dragend;
	GdkCursor *arrows;

	GtkAdjustment   *adj;
	gboolean         focus_on_cursor;

	/* Subwidgets */
	GtkWidget  *scrollbox;
	GtkWidget  *overlay;
	GtkWidget  *waveform;
	GtkWidget  *revealer;
	GtkWidget  *ruler;
	GtkWidget  *focus;
	GtkWidget  *cursor;
	GtkWidget  *selection;

	/* Event handling */
	GtkGesture *button;
	GtkEventController *motion_ctrl;
	GtkEventController *scroll_ctrl;

	guint       tick_handler;
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
	PROP_PPS,
	N_PROPERTIES
};

enum {
	CURSOR_CHANGED,
	FOLLOW_CURSOR_CHANGED,
	SELECTION_CHANGED,
	PLAY_TOGGLED,
	ZOOM_IN,
	ZOOM_OUT,
	LAST_SIGNAL
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewer, pt_waveviewer, GTK_TYPE_SCROLLED_WINDOW);


static gint64
time_to_pixel (PtWaveviewer *self,
               gint64        ms)
{
	/* Convert a time in 1/1000 seconds to the closest pixel in the drawing area */
	gint64 result;

	result = ms * self->priv->px_per_sec;
	result = result / 1000;

	return result;
}

static gint64
pixel_to_time (PtWaveviewer *self,
               gint64        pixel)
{
	/* Convert a position in the drawing area to time in milliseconds */
	gint64 result;

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
			/* cursor visible far left */
			gtk_adjustment_set_value (self->priv->adj, cursor_pos);
		}
	}
}

static void
render_cursor (PtWaveviewer *self)
{
	PtWaveviewerCursor *cursor;
	gint offset, pixel;

	cursor = PT_WAVEVIEWER_CURSOR (self->priv->cursor);
	offset = gtk_adjustment_get_value (self->priv->adj);
	pixel = time_to_pixel (self, self->priv->playback_cursor);

	pt_waveviewer_cursor_render (cursor, pixel - offset);
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
	/* add time to the cursor’s time so that the cursor is moved x pixels */

	gint64 result;
	gint   one_pixel;

	one_pixel = 1000 / self->priv->px_per_sec;
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
	PtWaveviewerSelection *sel_widget = PT_WAVEVIEWER_SELECTION (self->priv->selection);

	/* Is anything selected at all? */
	if (self->priv->dragstart == self->priv->dragend) {
		self->priv->sel_start = 0;
		self->priv->sel_end = 0;
		if (self->priv->has_selection) {
			self->priv->has_selection = FALSE;
			g_object_notify_by_pspec (G_OBJECT (self),
						  obj_properties[PROP_HAS_SELECTION]);
			g_signal_emit_by_name (self, "selection-changed");
			pt_waveviewer_selection_set (sel_widget, 0, 0);
		}
		return;
	}

	/* Don’t select too much */
	self->priv->dragstart = CLAMP (self->priv->dragstart, 0, self->priv->duration);
	self->priv->dragend = CLAMP (self->priv->dragend, 0, self->priv->duration);

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
		pt_waveviewer_selection_set (sel_widget,
					     time_to_pixel (self, self->priv->sel_start),
					     time_to_pixel (self, self->priv->sel_end));
	}
}

static gboolean
pt_waveviewer_key_press_event (GtkWidget   *widget,
                               GdkEventKey *event)
{
	PtWaveviewer    *self = PT_WAVEVIEWER (widget);
	GdkModifierType  state;
	guint            keyval;

	if (gdk_event_get_event_type ((GdkEvent*) event) != GDK_KEY_PRESS)
		return FALSE;

	if (self->priv->peaks->len == 0)
		return FALSE;

	gdk_event_get_state ((GdkEvent*) event, &state);
	gdk_event_get_keyval ((GdkEvent*) event, &keyval);

	/* no modifier pressed */
	if (!(state & ALL_ACCELS_MASK)) {
		switch (keyval) {
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
		if (!(state & ALL_ACCELS_MASK)) {

			switch (keyval) {
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
		else if ((state & ALL_ACCELS_MASK) == GDK_CONTROL_MASK) {

			switch (keyval) {
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

		/* No modifier pressed: Scroll window, depending on text direction */
		if (!(state & ALL_ACCELS_MASK)) {

			GtkScrollType scroll = GTK_SCROLL_NONE;
			switch (keyval) {
			case GDK_KEY_Left:
				scroll = GTK_SCROLL_STEP_BACKWARD;
				break;
			case GDK_KEY_Right:
				scroll = GTK_SCROLL_STEP_FORWARD;
				break;
			case GDK_KEY_Page_Up:
				scroll = GTK_SCROLL_PAGE_BACKWARD;
				break;
			case GDK_KEY_Page_Down:
				scroll = GTK_SCROLL_PAGE_FORWARD;
				break;
			case GDK_KEY_Home:
				scroll = GTK_SCROLL_START;
				break;
			case GDK_KEY_End:
				scroll = GTK_SCROLL_END;
				break;
			}
			if (scroll != GTK_SCROLL_NONE) {
				gboolean ret;
				g_signal_emit_by_name (self, "scroll-child", scroll, TRUE, &ret);
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
	gtk_widget_set_cursor (widget, cursor);
}

static gboolean
pt_waveviewer_button_press_event (GtkGestureClick *gesture,
                                  gint             n_press,
                                  gdouble          x,
                                  gdouble          y,
                                  gpointer         user_data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (user_data);
	GdkModifierType      state;
	guint                button;
	gint64               clicked;	/* the sample clicked on */
	gint64               pos;	/* clicked sample’s position in milliseconds */

	if (self->priv->peaks == NULL || self->priv->peaks->len == 0)
		return FALSE;

	if (!gtk_get_current_event_state (&state))
		return FALSE;

	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
	clicked = (gint) x;
	pos = pixel_to_time (self, clicked + gtk_adjustment_get_value (self->priv->adj));

	/* Single left click, no other keys pressed: new selection or changing selection */
	if (n_press == 1
	    && button == GDK_BUTTON_PRIMARY
	    && !(state & ALL_ACCELS_MASK)) {
		/* set position as start and end point, for new selection */
		self->priv->dragstart = self->priv->dragend = pos;

		/* if over selection border: snap to selection border, changing selection */
		if (pointer_in_range (self, x + gtk_adjustment_get_value (self->priv->adj), self->priv->sel_start)) {
			self->priv->dragstart = self->priv->sel_end;
			self->priv->dragend = self->priv->sel_start;
		} else if (pointer_in_range (self, x + gtk_adjustment_get_value (self->priv->adj), self->priv->sel_end)) {
			self->priv->dragstart = self->priv->sel_start;
			self->priv->dragend = self->priv->sel_end;
		}

		set_cursor (GTK_WIDGET (self), self->priv->arrows);
		update_selection (self);
		return TRUE;
	}

	/* Single left click with Shift pressed and existing selection: enlarge selection */
	if (n_press == 1
	    && button == GDK_BUTTON_PRIMARY
	    && (state & ALL_ACCELS_MASK) == GDK_SHIFT_MASK
	    && self->priv->sel_start != self->priv->sel_end) {

		self->priv->dragend = pos;

		if (pos < self->priv->sel_start)
			self->priv->dragstart = self->priv->sel_end;
		else
			self->priv->dragstart = self->priv->sel_start;

		set_cursor (GTK_WIDGET (self), self->priv->arrows);
		update_selection (self);
		return TRUE;
	}

	/* Single right click: change cursor */
	if (n_press == 1 && button == GDK_BUTTON_SECONDARY) {
		g_signal_emit_by_name (self, "cursor-changed", pos);
		return TRUE;
	}

	return FALSE;
}

static gboolean
pt_waveviewer_scroll_event (GtkEventControllerScroll *ctrl,
                            gdouble                   delta_x,
                            gdouble                   delta_y,
                            gpointer                  user_data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (user_data);
	GdkModifierType      state;
	GdkEvent            *event;

	event = gtk_get_current_event ();
	gtk_get_current_event_state (&state);

	/* No modifier pressed: scrolling back and forth */
	if (!(state & ALL_ACCELS_MASK)) {
		gtk_propagate_event
			(gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (self)),
			(GdkEvent*)event);
		gdk_event_free (event);
		return TRUE;
	}
	gdk_event_free (event);

	/* Only Control pressed: zoom in or out TODO handle this internally without signals*/
	if ((state & ALL_ACCELS_MASK) == GDK_CONTROL_MASK) {
		if (delta_y < 0 || delta_x < 0) {
			g_signal_emit_by_name (self, "zoom-out");
			return TRUE;
		}
		if (delta_y > 0 || delta_x > 0) {
			g_signal_emit_by_name (self, "zoom-in");
			return TRUE;
		}

	}

	return FALSE;
}

static gboolean
pt_waveviewer_motion_event (GtkEventControllerMotion *ctrl,
                            gdouble                   x,
			    gdouble                   y,
			    gpointer                  user_data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (user_data);
	GdkModifierType      state;
	gint64               clicked;	/* the sample clicked on */
	gint64               pos;	/* clicked sample’s position in milliseconds */

	gtk_get_current_event_state (&state);


	if (self->priv->peaks == NULL || self->priv->peaks->len == 0)
		return FALSE;

	clicked = (gint) x;
	pos = pixel_to_time (self, clicked + gtk_adjustment_get_value (self->priv->adj));

	/* Right mouse button sets cursor */
	if (state & GDK_BUTTON3_MASK) {
		g_signal_emit_by_name (self, "cursor-changed", pos);
		return TRUE;
	}

	/* Left mouse button (with or without Shift key) sets selection */
	if (state & GDK_BUTTON1_MASK || state & GDK_BUTTON1_MASK & GDK_SHIFT_MASK) {
		self->priv->dragend = pos;
		update_selection (self);
		return TRUE;
	}

	/* No button or any other button: change pointer cursor over selection border */
	if (self->priv->sel_start != self->priv->sel_end) {
		if (pointer_in_range (self, x + gtk_adjustment_get_value (self->priv->adj), self->priv->sel_start)
		    || pointer_in_range (self, x + gtk_adjustment_get_value (self->priv->adj), self->priv->sel_end)) {
			set_cursor (GTK_WIDGET (self), self->priv->arrows);
		} else {
			set_cursor (GTK_WIDGET (self), NULL);
		}
	}

	return FALSE;
}

static gboolean
pt_waveviewer_button_release_event (GtkGestureClick *gesture,
                                    gint             n_press,
                                    gdouble          x,
                                    gdouble          y,
                                    gpointer         user_data)
{
	guint button;

	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
	if (n_press == 1 && button == GDK_BUTTON_PRIMARY) {
		set_cursor (GTK_WIDGET (user_data), NULL);
		return TRUE;
	}
	return FALSE;
}

static void
stop_following_cursor (PtWaveviewer *self)
{
	if (self->priv->peaks)
		pt_waveviewer_set_follow_cursor (self, FALSE);
}

static gboolean
scrollbar_button_press_event_cb (GtkWidget      *widget,
                                 GdkEventButton *event,
                                 gpointer        data)
{
	/* If user clicks on scrollbar don’t follow cursor anymore.
	   Otherwise it would scroll immediately back again. */

	stop_following_cursor (PT_WAVEVIEWER (data));

	/* Propagate signal */
	return FALSE;
}

static gboolean
scrollbar_scroll_event_cb (GtkWidget      *widget,
                           GdkEventButton *event,
                           gpointer        data)
{
	/* If user scrolls on scrollbar don’t follow cursor anymore.
	   Otherwise it would scroll immediately back again. */

	stop_following_cursor (PT_WAVEVIEWER (data));

	/* Propagate signal */
	return FALSE;
}

static void
focus_cursor (PtWaveviewer *self)
{
	self->priv->focus_on_cursor = TRUE;
	pt_waveviewer_cursor_set_focus (PT_WAVEVIEWER_CURSOR  (self->priv->cursor), TRUE);
	pt_waveviewer_focus_set (PT_WAVEVIEWER_FOCUS  (self->priv->focus), FALSE);
}

static void
focus_widget (PtWaveviewer *self)
{
	self->priv->focus_on_cursor = FALSE;
	pt_waveviewer_cursor_set_focus (PT_WAVEVIEWER_CURSOR  (self->priv->cursor), FALSE);
	pt_waveviewer_focus_set (PT_WAVEVIEWER_FOCUS  (self->priv->focus), TRUE);
}

static void
focus_lost (PtWaveviewer *self)
{
	self->priv->focus_on_cursor = FALSE;
	pt_waveviewer_cursor_set_focus (PT_WAVEVIEWER_CURSOR  (self->priv->cursor), FALSE);
	pt_waveviewer_focus_set (PT_WAVEVIEWER_FOCUS  (self->priv->focus), FALSE);
}

static gboolean
pt_waveviewer_focus_out_event (GtkWidget *widget,
                               GdkEvent  *event,
                               gpointer   data)
{
	focus_lost (PT_WAVEVIEWER (widget));
	return FALSE;
}

static gboolean
pt_waveviewer_focus_in_event (GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (widget);
	if (!self->priv->focus_on_cursor && self->priv->peaks->len > 0)
		focus_widget (self);
	return FALSE;
}

static gboolean
pt_waveviewer_focus (GtkWidget        *widget,
                     GtkDirectionType  direction,
                     gpointer          data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (widget);

	/* See API reference for gtk_widget_child_focus ():
	   If moving into @direction would move focus outside of widget: return FALSE;
	   If moving into @direction would stay inside widget: return TRUE */

	/* Don’t focus if empty */
	if (self->priv->peaks == NULL || self->priv->peaks->len == 0)
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
 *
 * Since: 1.5
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
 *
 * Since: 1.5
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
		g_signal_emit_by_name (self, "follow-cursor-changed", self->priv->follow_cursor);
		if (follow)
			scroll_to_cursor (self);
	}
}

static gboolean
cursor_is_visible (PtWaveviewer *self)
{
	gint cursor_pos, left, right;

	cursor_pos = time_to_pixel (self, self->priv->playback_cursor);
	left = (gint) gtk_adjustment_get_value (self->priv->adj);
	right = left + (gint) gtk_adjustment_get_page_size (self->priv->adj);

	return (left <= cursor_pos && cursor_pos <= right);
}

static void
get_anchor_point (PtWaveviewer *self)
{
	/* Get an anchor point = zoom_pos, either cursor position or
	   middle of view. The anchor point is relative to the view.
	   Get time of that anchor point = zoom_time. */

	gint left;

	left = (gint) gtk_adjustment_get_value (self->priv->adj);
	if (cursor_is_visible (self)) {
		self->priv->zoom_time = self->priv->playback_cursor;
		self->priv->zoom_pos = time_to_pixel (self, self->priv->playback_cursor) - left;
	} else {
		self->priv->zoom_pos = (gint) gtk_adjustment_get_page_size (self->priv->adj) / 2;
		self->priv->zoom_time = pixel_to_time (self, self->priv->zoom_pos + left);
	}
}

static void
reset_selection (PtWaveviewer *self)
{
	PtWaveviewerPrivate *priv = self->priv;

	if (priv->has_selection == FALSE)
		return;

	g_object_freeze_notify (G_OBJECT (self));

	priv->has_selection = FALSE;
	g_object_notify_by_pspec (G_OBJECT (self),
	                          obj_properties[PROP_HAS_SELECTION]);

	if (priv->sel_start > 0) {
		priv->sel_start = 0;
		g_object_notify_by_pspec (G_OBJECT (self),
		                          obj_properties[PROP_SELECTION_START]);
	}

	priv->sel_end = 0;
	g_object_notify_by_pspec (G_OBJECT (self),
	                          obj_properties[PROP_SELECTION_END]);

	g_object_thaw_notify (G_OBJECT (self));
}

static void
array_size_changed_cb (PtWaveloader *loader,
		       gpointer      user_data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (user_data);
	gint width;

	if (self->priv->peaks == NULL || self->priv->peaks->len == 0) {
		self->priv->peaks_size = 0;
		self->priv->px_per_sec = 0;
		self->priv->duration = 0;
		pt_waveviewer_selection_set (
				PT_WAVEVIEWER_SELECTION (self->priv->selection),
				0, 0);
		pt_waveviewer_cursor_render (
				PT_WAVEVIEWER_CURSOR (self->priv->cursor),
				-1);
	} else {
		self->priv->peaks_size = self->priv->peaks->len;
		self->priv->px_per_sec = self->priv->pps;
		self->priv->duration = calculate_duration (self);
	}

	width = self->priv->peaks_size / 2;
	pt_waveviewer_scrollbox_set (PT_WAVEVIEWER_SCROLLBOX (self->priv->scrollbox), width);

	pt_waveviewer_ruler_set_ruler (
			PT_WAVEVIEWER_RULER (self->priv->ruler),
			self->priv->peaks_size / 2,
			self->priv->px_per_sec,
			self->priv->duration);
}

static void
propagate_progress_cb (PtWaveloader *loader,
                       gdouble       progress,
                       PtWaveviewer *self)
{
	g_signal_emit_by_name (self, "load-progress", progress);
}

static void
load_cb (PtWaveloader *loader,
         GAsyncResult *res,
         gpointer     *data)
{
	GTask        *task = (GTask *) data;
	PtWaveviewer *self = g_task_get_source_object (task);
	GError       *error = NULL;

	if (self->priv->tick_handler != 0) {
		gtk_widget_remove_tick_callback (self->priv->waveform,
						 self->priv->tick_handler);
		self->priv->tick_handler = 0;
	}

	if (pt_waveloader_load_finish (loader, res, &error)) {
		array_size_changed_cb (NULL, self);
		gtk_widget_queue_draw (self->priv->waveform);
		render_cursor (self);
		g_task_return_boolean (task, TRUE);
	} else {
		array_size_changed_cb (NULL, self);
		g_task_return_error (task, error);
	}

	g_object_unref (task);
}

/**
 * pt_waveviewer_load_wave_finish:
 * @self: the widget
 * @result: the #GAsyncResult passed to your #GAsyncReadyCallback
 * @error: (nullable): a pointer to a NULL #GError, or NULL
 *
 * Gives the result of the async load operation. A cancelled operation results
 * in an error, too.
 *
 * Return value: TRUE if successful, or FALSE with error set
 *
 * Since: 2.0
 */
gboolean
pt_waveviewer_load_wave_finish (PtWaveviewer  *self,
                                GAsyncResult  *result,
                                GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
update_waveform_cb (GtkWidget     *widget,
                    GdkFrameClock *frame_clock,
                    gpointer       data)
{
	gtk_widget_queue_draw (widget);
	return G_SOURCE_CONTINUE;
}

/**
 * pt_waveviewer_load_wave_async:
 * @self: the widget
 * @uri: the URI of the file
 * @cancel: (nullable): a #GCancellable or NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the operation is complete
 * @user_data: (closure): user data for callback
 *
 * Load wave form for the given URI. The initial resolution is set to
 * #PtWaveviewer:pps. While loading, a #PtWaveviewer::load-progress signal is
 * emitted. A previous waveform is discarded.
 *
 * Since: 2.0
 */
void
pt_waveviewer_load_wave_async (PtWaveviewer        *self,
                               gchar               *uri,
                               GCancellable        *cancel,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
	g_return_if_fail (PT_IS_WAVEVIEWER (self));
	g_return_if_fail (uri != NULL);

	GTask  *task;
	/* TODO think about doing this here or just returning error from waveloader.
	 * this is meant for "nicer" error messages, requires translation
	GFile  *file;
	gchar  *location = NULL;*/

	task = g_task_new (self, NULL, callback, user_data);

#if 0
	/* Change uri to location */
	file = g_file_new_for_uri (uri);
	location = g_file_get_path (file);

	if (!location) {
		g_task_return_new_error (
				task,
				GST_RESOURCE_ERROR,
				GST_RESOURCE_ERROR_NOT_FOUND,
				/* Translators: %s is a detailed error message. */
				_("URI not valid: %s"), uri);
		g_object_unref (file);
		g_object_unref (task);
		return;
	}

	if (!g_file_query_exists (file, NULL)) {
		g_task_return_new_error (
				task,
				GST_RESOURCE_ERROR,
				GST_RESOURCE_ERROR_NOT_FOUND,
				/* Translators: %s is a detailed error message. */
				_("File not found: %s"), location);
		g_object_unref (file);
		g_free (location);
		g_object_unref (task);
		return;
	}

	g_object_unref (file);
	g_free (location);
#endif

	reset_selection (self);

	g_object_set (self->priv->loader, "uri", uri, NULL);
	self->priv->px_per_sec = self->priv->pps;
	if (self->priv->tick_handler == 0) {
		self->priv->tick_handler =
			gtk_widget_add_tick_callback (
				self->priv->waveform,
				(GtkTickCallback) update_waveform_cb,
				self, NULL);
	}
	pt_waveloader_load_async (self->priv->loader,
				  self->priv->pps,
				  cancel,
				  (GAsyncReadyCallback) load_cb,
				  task);
}

static void
pt_waveviewer_finalize (GObject *object)
{
	PtWaveviewer *self = PT_WAVEVIEWER (object);

	g_clear_object (&self->priv->arrows);
	g_clear_object (&self->priv->loader);
	if (self->priv->tick_handler != 0) {
		gtk_widget_remove_tick_callback (self->priv->waveform,
						 self->priv->tick_handler);
		self->priv->tick_handler = 0;
	}

	G_OBJECT_CLASS (pt_waveviewer_parent_class)->finalize (object);
}

static void
pt_waveviewer_dispose (GObject *object)
{
	G_OBJECT_CLASS (pt_waveviewer_parent_class)->dispose (object);
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
	case PROP_PPS:
		g_value_set_int (value, self->priv->pps);
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
	GError *error = NULL;

	switch (property_id) {
	case PROP_PLAYBACK_CURSOR:
		/* ignore if cursor’s value didn’t change */
		if (self->priv->playback_cursor == g_value_get_int64 (value))
			break;
		self->priv->playback_cursor = g_value_get_int64 (value);

		/* ignore if we’re not realized yet, widget is in construction */
		if (!gtk_widget_get_realized (GTK_WIDGET (self)))
			break;

		if (self->priv->follow_cursor)
			scroll_to_cursor (self);

		render_cursor (self);
		break;
	case PROP_FOLLOW_CURSOR:
		self->priv->follow_cursor = g_value_get_boolean (value);
		if (gtk_widget_get_realized (GTK_WIDGET (self)) && self->priv->follow_cursor)
			scroll_to_cursor (self);
		g_signal_emit_by_name (self, "follow-cursor-changed", self->priv->follow_cursor);
		break;
	case PROP_FIXED_CURSOR:
		self->priv->fixed_cursor = g_value_get_boolean (value);
		break;
	case PROP_SHOW_RULER:
		self->priv->show_ruler = g_value_get_boolean (value);
		gtk_revealer_set_reveal_child (GTK_REVEALER (self->priv->revealer),
					       self->priv->show_ruler);
		break;
	case PROP_PPS:
		self->priv->pps = g_value_get_int (value);
		if (self->priv->peaks->len == 0)
			break;
		get_anchor_point (self);
		if (!pt_waveloader_resize (self->priv->loader,
				           self->priv->pps,
				           &error)) {
			g_print ("%s\n", error->message);
			g_clear_error (&error);
			break;
		}

		array_size_changed_cb (NULL, self);
		gtk_adjustment_set_value (self->priv->adj, time_to_pixel (self, self->priv->zoom_time) - self->priv->zoom_pos);
		gtk_widget_queue_draw (self->priv->waveform);
		render_cursor (self);
		if (self->priv->has_selection) {
			pt_waveviewer_selection_set (PT_WAVEVIEWER_SELECTION (self->priv->selection),
					time_to_pixel (self, self->priv->sel_start),
					time_to_pixel (self, self->priv->sel_end));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             gpointer       user_data)
{
	PtWaveviewer *self = PT_WAVEVIEWER (user_data);

	render_cursor (self);
}

static void
pt_waveviewer_constructed (GObject *object)
{
	PtWaveviewer *self = PT_WAVEVIEWER (object);

	GtkWidget *scrollbar = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (self));
	self->priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (self));

	g_signal_connect (self->priv->adj,
			  "value_changed",
			  G_CALLBACK (adjustment_value_changed_cb),
			  self);
	g_signal_connect (scrollbar,
			  "button_press_event",
			  G_CALLBACK (scrollbar_button_press_event_cb),
			  self);
	g_signal_connect (scrollbar,
			  "scroll_event",
			  G_CALLBACK (scrollbar_scroll_event_cb),
			  self);
}

static GdkCursor *
get_resize_cursor (void)
{
	GdkCursor  *result;

	result = gdk_cursor_new_from_name ("ew-resize", NULL);
	if (!result)
		result = gdk_cursor_new_from_name ("col-resize", NULL);

	return result;
}

static void
pt_waveviewer_init (PtWaveviewer *self)
{
	self->priv = pt_waveviewer_get_instance_private (self);

	g_type_ensure (PT_TYPE_WAVEVIEWER_SCROLLBOX);
	g_type_ensure (PT_TYPE_WAVEVIEWER_RULER);
	g_type_ensure (PT_TYPE_WAVEVIEWER_WAVEFORM);
	g_type_ensure (PT_TYPE_WAVEVIEWER_SELECTION);
	g_type_ensure (PT_TYPE_WAVEVIEWER_CURSOR);
	g_type_ensure (PT_TYPE_WAVEVIEWER_FOCUS);

	gtk_widget_init_template (GTK_WIDGET (self));

	GtkCssProvider  *provider;
	GFile		*css_file;

	self->priv->peaks_size = 0;
	self->priv->duration = 0;
	self->priv->playback_cursor = 0;
	self->priv->follow_cursor = TRUE;
	self->priv->focus_on_cursor = FALSE;
	self->priv->sel_start = 0;
	self->priv->sel_end = 0;
	self->priv->zoom_time = 0;
	self->priv->zoom_pos = 0;
	self->priv->arrows = get_resize_cursor ();
	self->priv->loader = pt_waveloader_new (NULL);
	self->priv->peaks = pt_waveloader_get_data (self->priv->loader);
	self->priv->tick_handler = 0;

	pt_waveviewer_waveform_set (
			PT_WAVEVIEWER_WAVEFORM (self->priv->waveform),
			self->priv->peaks);

	css_file = g_file_new_for_uri ("resource:///org/parlatype/libparlatype/pt-waveviewer.css");
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_file (provider, css_file);
	gtk_style_context_add_provider_for_display (
			gdk_display_get_default (),
			GTK_STYLE_PROVIDER (provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_object_unref (css_file);
	g_object_unref (provider);

	/* Setup event handling */
	self->priv->button = gtk_gesture_click_new ();
	gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (self->priv->button), TRUE);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->priv->button), 0);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->priv->button), GTK_PHASE_CAPTURE);
	g_signal_connect (
			self->priv->button,
			"pressed",
			G_CALLBACK (pt_waveviewer_button_press_event),
			self);
	g_signal_connect (
			self->priv->button,
			"released",
			G_CALLBACK (pt_waveviewer_button_release_event),
			self);
	gtk_widget_add_controller (self->priv->scrollbox, GTK_EVENT_CONTROLLER (self->priv->button));

	self->priv->motion_ctrl = gtk_event_controller_motion_new ();
	g_signal_connect (
			self->priv->motion_ctrl,
			"motion",
			G_CALLBACK (pt_waveviewer_motion_event),
			self);
	gtk_widget_add_controller (self->priv->scrollbox, self->priv->motion_ctrl);

	self->priv->scroll_ctrl = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL | GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->priv->scroll_ctrl), GTK_PHASE_CAPTURE);
	g_signal_connect (
			self->priv->scroll_ctrl,
			"scroll",
			G_CALLBACK (pt_waveviewer_scroll_event),
			self);
	gtk_widget_add_controller (GTK_WIDGET (self), self->priv->scroll_ctrl);

	/* If overriding these vfuncs something’s going wrong, note that focus-in
	   an focus-out need GdkEventFocus as 2nd parameter in vfunc */
	g_signal_connect (self, "focus", G_CALLBACK (pt_waveviewer_focus), NULL);
	g_signal_connect (self, "focus-in-event", G_CALLBACK (pt_waveviewer_focus_in_event), NULL);
	g_signal_connect (self, "focus-out-event", G_CALLBACK (pt_waveviewer_focus_out_event), NULL);

	g_signal_connect (self->priv->loader,
			"progress",
			G_CALLBACK (propagate_progress_cb),
			self);

	g_signal_connect (self->priv->loader,
			"array-size-changed",
			G_CALLBACK (array_size_changed_cb),
			self);

}

static void
pt_waveviewer_class_init (PtWaveviewerClass *klass)
{
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = pt_waveviewer_set_property;
	gobject_class->get_property = pt_waveviewer_get_property;
	gobject_class->constructed  = pt_waveviewer_constructed;
	gobject_class->dispose      = pt_waveviewer_dispose;
	gobject_class->finalize     = pt_waveviewer_finalize;

	widget_class->key_press_event      = pt_waveviewer_key_press_event;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/libparlatype/pt-waveviewer.ui");
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, scrollbox);
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, revealer);
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, ruler);
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, overlay);
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, waveform);
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, selection);
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, cursor);
	gtk_widget_class_bind_template_child_private (widget_class, PtWaveviewer, focus);

	/**
	* PtWaveviewer::load-progress:
	* @viewer: the waveviewer emitting the signal
	* @progress: the new progress state, ranging from 0.0 to 1.0
	*
	* Indicates progress on a scale from 0.0 to 1.0, however it does not
	* emit the value 0.0 nor 1.0. Wait for a TRUE player-state-changed
	* signal or an error signal to dismiss a gui element showing progress.
	*/
	g_signal_new ("load-progress",
		      PT_TYPE_WAVEVIEWER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__DOUBLE,
		      G_TYPE_NONE,
		      1, G_TYPE_DOUBLE);

	/**
	* PtWaveviewer::cursor-changed:
	* @viewer: the waveviewer emitting the signal
	* @position: the new position in stream in milliseconds
	*
	* Signals that the cursor’s position was changed by the user.
	*/
	signals[CURSOR_CHANGED] =
	g_signal_new ("cursor-changed",
		      PT_TYPE_WAVEVIEWER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      _pt_cclosure_marshal_VOID__INT64,
		      G_TYPE_NONE,
		      1, G_TYPE_INT64);

	/**
	* PtWaveviewer::follow-cursor-changed:
	* @viewer: the waveviewer emitting the signal
	* @follow: the new value
	*
	* Signals that the #PtWaveviewer:follow-cursor property has changed.
	*/
	signals[FOLLOW_CURSOR_CHANGED] =
	g_signal_new ("follow-cursor-changed",
		      PT_TYPE_WAVEVIEWER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__BOOLEAN,
		      G_TYPE_NONE,
		      1, G_TYPE_BOOLEAN);


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
	* PtWaveviewer::zoom-in:
	* @viewer: the waveviewer emitting the signal
	*
	* Signals that the user requested to zoom into the waveform.
	*/
	signals[ZOOM_IN] =
	g_signal_new ("zoom-in",
		      PT_TYPE_WAVEVIEWER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	/**
	* PtWaveviewer::zoom-out:
	* @viewer: the waveviewer emitting the signal
	*
	* Signals that the user requested to zoom out of the waveform.
	*/
	signals[ZOOM_OUT] =
	g_signal_new ("zoom-out",
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
	 * Current playback position in milliseconds. A value of -1 means an
	 * invalid or unknown position that will not be rendered.
	 */

	obj_properties[PROP_PLAYBACK_CURSOR] =
	g_param_spec_int64 (
			"playback-cursor",
			"Cursor position",
			"Cursor’s position in 1/1000 seconds",
			-1,
			G_MAXINT64,
			0,
			G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

	/**
	* PtWaveviewer:follow-cursor:
	*
	* If the widget follows the cursor, it scrolls automatically to the
	* cursor’s position. Note that the widget will change this property to
	* FALSE if the user scrolls the widget manually.
	*/

	obj_properties[PROP_FOLLOW_CURSOR] =
	g_param_spec_boolean (
			"follow-cursor",
			_("Follow cursor"),
			_("Scroll automatically to the cursor’s position"),
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
	* Start time of selection in milliseconds. If it’s equal to the end
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
	* End time of selection in milliseconds. If it’s equal to the start
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

	/**
	* PtWaveviewer:pps:
	*
	* Current/requested resolution of waveform in pixels per second.
	*/

	obj_properties[PROP_PPS] =
	g_param_spec_int (
			"pps",
			"Pixels per second",
			"Current/requested resolution of waveform in pixels per second",
			25,
			200,
			100,
			G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (
			gobject_class,
			N_PROPERTIES,
			obj_properties);
}

/**
 * pt_waveviewer_new:
 *
 * Create a new, initially blank waveform viewer widget.
 *
 * After use gtk_widget_destroy() it.
 *
 * Returns: the widget
 *
 * Since: 1.5
 */
GtkWidget *
pt_waveviewer_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER, NULL));
}
