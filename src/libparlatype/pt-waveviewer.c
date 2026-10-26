/* Copyright (C) 2006-2008 Buzztrax team <buzztrax-devel@buzztrax.org>
 * Copyright (C) 2002 2003 2004 2005 2007 2008 2009 2010, Magnus Hjorth
 * Copyright 2016 Gabor Karsay <gabor.karsay@gmx.at>
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
 * PtWaveviewer:
 *
 * Widget to display a waveform.
 *
 * Displays a waveform and lets the user interact with it: jumping to a position,
 * make selections and so on.
 */

#define G_LOG_DOMAIN "pt-waveviewer"

#include "config.h"

#include "pt-waveviewer.h"

#include "pt-marshalers.h"
#include "pt-waveloader.h"
#include "pt-waveviewer-cursor.h"
#include "pt-waveviewer-ruler.h"
#include "pt-waveviewer-scrollbox.h"
#include "pt-waveviewer-selection.h"
#include "pt-waveviewer-waveform.h"

#include <glib/gi18n.h>
#include <math.h> /* fabs */
#include <string.h>

#define ALL_ACCELS_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK)
#define CURSOR_POSITION 0.5
#define WAVE_MIN_HEIGHT 20
#define PPS_MIN 25
#define PPS_MAX 200

struct _PtWaveviewer
{
  GtkWidget parent_instance;

  /* Wavedata */
  PtWaveloader *loader;
  GArray       *peaks;
  gint64        peaks_size; /* size of array */
  gint          px_per_sec;
  gint64        duration; /* in milliseconds */

  /* Properties */
  gint64   playback_cursor;
  gboolean follow_cursor;
  gboolean fixed_cursor;
  gboolean show_ruler;
  gboolean has_selection;
  gint     pps;

  gint64 zoom_time;
  gint   zoom_pos;

  /* Selections, in milliseconds */
  gint64     sel_start;
  gint64     sel_end;
  gdouble    drag_start_0;
  gdouble    drag_start_x;
  gint64     drag_fixed_pos;
  gint64     drag_moving_pos;
  GdkCursor *arrows;

  GtkAdjustment *adj;

  /* Subwidgets */
  GtkWidget *scrollbox;
  GtkWidget *overlay;
  GtkWidget *scrolled_window;
  GtkWidget *waveform;
  GtkWidget *revealer;
  GtkWidget *ruler;
  GtkWidget *cursor;
  GtkWidget *selection;

  /* Event handling */
  GtkGesture         *button;
  GtkEventController *motion_ctrl;
  GtkEventController *scroll_ctrl;
  GtkEventController *key_ctrl;
  GtkEventController *focus_ctrl;

  guint tick_handler;
};

enum
{
  PROP_PLAYBACK_CURSOR = 1,
  PROP_FOLLOW_CURSOR,
  PROP_FIXED_CURSOR,
  PROP_SHOW_RULER,
  PROP_HAS_SELECTION,
  PROP_SELECTION_START,
  PROP_SELECTION_END,
  PROP_PPS,
  N_PROPERTIES
};

enum
{
  CURSOR_CHANGED,
  SELECTION_CHANGED,
  PLAY_TOGGLED,
  LAST_SIGNAL
};

static GParamSpec *obj_properties[N_PROPERTIES];
static guint       signals[LAST_SIGNAL] = { 0 };
static void        pt_waveviewer_set_pps (PtWaveviewer *self, int pps);

G_DEFINE_FINAL_TYPE (PtWaveviewer, pt_waveviewer, GTK_TYPE_WIDGET);

static gint64
time_to_pixel (PtWaveviewer *self,
               gint64        ms)
{
  /* Convert a time in 1/1000 seconds to the closest pixel in the drawing area */
  gint64 result;

  result = ms * self->px_per_sec;
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
  result = result / self->px_per_sec;

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

  cursor_pos = time_to_pixel (self, self->playback_cursor);
  first_visible = (gint) gtk_adjustment_get_value (self->adj);
  page_width = (gint) gtk_adjustment_get_page_size (self->adj);
  last_visible = first_visible + page_width;

  /* Fixed cursor: always scroll,
     non-fixed cursor: only scroll if cursor is not visible */

  if (self->fixed_cursor)
    {
      offset = page_width * CURSOR_POSITION;
      gtk_adjustment_set_value (self->adj, cursor_pos - offset);
    }
  else
    {
      if (cursor_pos < first_visible || cursor_pos > last_visible)
        {
          /* cursor visible far left */
          gtk_adjustment_set_value (self->adj, cursor_pos);
        }
    }
}

static void
render_cursor (PtWaveviewer *self)
{
  PtWaveviewerCursor *cursor;
  gint                offset, pixel;

  cursor = PT_WAVEVIEWER_CURSOR (self->cursor);
  offset = gtk_adjustment_get_value (self->adj);
  pixel = time_to_pixel (self, self->playback_cursor);

  pt_waveviewer_cursor_render (cursor, pixel - offset);
}

static gint64
calculate_duration (PtWaveviewer *self)
{
  gint64 result;
  gint64 samples;
  gint   one_pixel;

  one_pixel = 1000 / self->px_per_sec;
  samples = self->peaks_size / 2;
  result = samples / self->px_per_sec * 1000; /* = full seconds */
  result += (samples % self->px_per_sec) * one_pixel;

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

  one_pixel = 1000 / self->px_per_sec;
  result = self->playback_cursor + pixel * one_pixel;

  /* Return result in range */
  if (stay_in_selection && self->has_selection)
    return CLAMP (result, self->sel_start, self->sel_end);
  else
    return CLAMP (result, 0, self->duration);
}

static void
update_selection (PtWaveviewer *self)
{
  /* Check if drag positions are different from selection.
     If yes, set new selection and emit signals. */

  /* Is anything selected at all? */
  if (self->drag_fixed_pos == self->drag_moving_pos)
    {
      self->sel_start = 0;
      self->sel_end = 0;
      if (self->has_selection)
        {
          self->has_selection = FALSE;
          g_object_notify_by_pspec (G_OBJECT (self),
                                    obj_properties[PROP_HAS_SELECTION]);
          g_signal_emit_by_name (self, "selection-changed");
        }
      return;
    }

  /* Sort start/end, check for changes, update selection */
  if (self->drag_fixed_pos < self->drag_moving_pos)
    {
      if (self->sel_start == self->drag_fixed_pos && self->sel_end == self->drag_moving_pos)
        {
          return;
        }
      self->sel_start = self->drag_fixed_pos;
      self->sel_end = self->drag_moving_pos;
    }
  else
    {
      if (self->sel_start == self->drag_moving_pos && self->sel_end == self->drag_fixed_pos)
        {
          return;
        }
      self->sel_start = self->drag_moving_pos;
      self->sel_end = self->drag_fixed_pos;
    }

  /* Update has-selection property */
  if (!self->has_selection)
    {
      self->has_selection = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self),
                                obj_properties[PROP_HAS_SELECTION]);
    }

  g_signal_emit_by_name (self, "selection-changed");
}

static gboolean
pt_waveviewer_key_press_event (GtkEventControllerKey *ctrl,
                               guint                  keyval,
                               guint                  keycode,
                               GdkModifierType        state,
                               gpointer               user_data)
{
  PtWaveviewer *self = PT_WAVEVIEWER (user_data);

  if (self->peaks->len == 0)
    return FALSE;

  /* no modifier pressed */
  if (!(state & ALL_ACCELS_MASK))
    {
      switch (keyval)
        {
        case GDK_KEY_Escape:
          self->drag_fixed_pos = self->drag_moving_pos = 0;
          gtk_widget_set_cursor (GTK_WIDGET (self->cursor), NULL);
          pt_waveviewer_selection_set (PT_WAVEVIEWER_SELECTION (self->selection), 0, 0);
          update_selection (self);
          return TRUE;
        case GDK_KEY_space:
          g_signal_emit_by_name (self, "play-toggled");
          return TRUE;
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
          if (self->has_selection)
            g_signal_emit_by_name (self, "cursor-changed", self->sel_start);
          else
            g_signal_emit_by_name (self, "cursor-changed", 0);
          return TRUE;
        case GDK_KEY_End:
          if (self->has_selection)
            g_signal_emit_by_name (self, "cursor-changed", self->sel_end);
          else
            g_signal_emit_by_name (self, "cursor-changed", self->duration);
          return TRUE;
        default:
          return FALSE;
        }
    }

  if ((state & ALL_ACCELS_MASK) == GDK_CONTROL_MASK)
    {

      GtkScrollType scroll = GTK_SCROLL_NONE;
      switch (keyval)
        {
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
        default:
          return FALSE;
        }

      gboolean ret;
      g_signal_emit_by_name (self->scrolled_window, "scroll-child", scroll, TRUE, &ret);
      pt_waveviewer_set_follow_cursor (self, FALSE);
      return TRUE;
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

static gboolean
pt_waveviewer_button_press_event (GtkGestureClick *gesture,
                                  gint             n_press,
                                  gdouble          x,
                                  gdouble          y,
                                  gpointer         user_data)
{
  PtWaveviewer   *self = PT_WAVEVIEWER (user_data);
  GdkModifierType state;
  guint           button;
  gint64          pos; /* clicked sample’s position in milliseconds */

  if (self->peaks == NULL || self->peaks->len == 0)
    return FALSE;

  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  pos = pixel_to_time (self, (gint) x + gtk_adjustment_get_value (self->adj));

  /* Single left click with Shift pressed and existing selection: change selection */
  if (n_press == 1 &&
      button == GDK_BUTTON_PRIMARY &&
      (state & ALL_ACCELS_MASK) == GDK_SHIFT_MASK &&
      self->sel_start != self->sel_end)
    {
      /* Change selection: move the end that is closer to the clicked pixel.
       * This can make the selection larger or smaller. */
      pos = CLAMP (pos, 0, self->duration);
      if (ABS (self->drag_fixed_pos - pos) < ABS (self->drag_moving_pos - pos))
        self->drag_fixed_pos = pos;
      else
        self->drag_moving_pos = pos;

      gtk_widget_set_cursor (GTK_WIDGET (self->cursor), self->arrows);
      pt_waveviewer_selection_set (PT_WAVEVIEWER_SELECTION (self->selection),
                                   time_to_pixel (self, self->drag_fixed_pos),
                                   time_to_pixel (self, self->drag_moving_pos));
      update_selection (self);
      gtk_widget_grab_focus (GTK_WIDGET (self));
      return TRUE;
    }

  /* Single right click: change cursor */
  if (n_press == 1 && button == GDK_BUTTON_SECONDARY)
    {
      g_signal_emit_by_name (self, "cursor-changed", pos);
      gtk_widget_grab_focus (GTK_WIDGET (self));
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
  PtWaveviewer   *self = PT_WAVEVIEWER (user_data);
  GdkModifierType state;

  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (ctrl));

  /* No modifier pressed: scrolling back and forth */
  if (!(state & ALL_ACCELS_MASK))
    {
      GtkScrollType scroll = GTK_SCROLL_NONE;
      gboolean      ret;
      if (delta_y < 0 || delta_x < 0)
        scroll = GTK_SCROLL_STEP_BACKWARD;
      if (delta_y > 0 || delta_x > 0)
        scroll = GTK_SCROLL_STEP_FORWARD;
      if (scroll != GTK_SCROLL_NONE)
        {
          g_signal_emit_by_name (self->scrolled_window, "scroll-child", scroll, TRUE, &ret);
          pt_waveviewer_set_follow_cursor (self, FALSE);
          return TRUE;
        }
    }

  if ((state & ALL_ACCELS_MASK) == GDK_CONTROL_MASK)
    {
      if (delta_y < 0 || delta_x < 0)
        {
          pt_waveviewer_set_pps (self, self->pps - 10);
          return TRUE;
        }
      if (delta_y > 0 || delta_x > 0)
        {
          pt_waveviewer_set_pps (self, self->pps + 10);
          return TRUE;
        }
    }

  return FALSE;
}

static void
pt_waveviewer_motion_event (GtkEventControllerMotion *ctrl,
                            gdouble                   x,
                            gdouble                   y,
                            gpointer                  user_data)
{
  PtWaveviewer *self = PT_WAVEVIEWER (user_data);
  gboolean      have_selection, currently_dragging;
  gdouble       real_x;

  if (self->peaks == NULL || self->peaks->len == 0)
    return;

  have_selection = (self->sel_start != self->sel_end);
  currently_dragging = (self->drag_start_x >= 0);
  real_x = x + gtk_adjustment_get_value (self->adj);

  if (!have_selection || currently_dragging)
    return;

  /* Change pointer cursor over selection border. */
  if (pointer_in_range (self, real_x, self->sel_start) ||
      pointer_in_range (self, real_x, self->sel_end))
    {
      gtk_widget_set_cursor (GTK_WIDGET (self->cursor), self->arrows);
    }
  else
    {
      gtk_widget_set_cursor (GTK_WIDGET (self->cursor), NULL);
    }
}

static void
pt_waveviewer_drag_begin (GtkGestureDrag *ctrl,
                          gdouble         start_x,
                          gdouble         start_y,
                          gpointer        user_data)
{
  PtWaveviewer *self = PT_WAVEVIEWER (user_data);
  gint64        pos; /* clicked sample’s position in milliseconds */
  gdouble       x;

  if (self->peaks == NULL || self->peaks->len == 0)
    return;

  /* Set position as start and end point for a new selection. */
  self->drag_start_0 = gtk_adjustment_get_value (self->adj);
  self->drag_start_x = start_x;
  x = self->drag_start_0 + self->drag_start_x;
  pos = CLAMP (pixel_to_time (self, x), 0, self->duration);
  self->drag_fixed_pos = self->drag_moving_pos = pos;

  /* if over selection border: snap to selection border, changing selection */
  if (pointer_in_range (self, x, self->sel_start))
    {
      self->drag_moving_pos = self->sel_start;
      self->drag_fixed_pos = self->sel_end;
    }
  else if (pointer_in_range (self, x, self->sel_end))
    {
      self->drag_moving_pos = self->sel_end;
      self->drag_fixed_pos = self->sel_start;
    }

  gtk_widget_set_cursor (GTK_WIDGET (self->cursor), self->arrows);
  gtk_widget_grab_focus (GTK_WIDGET (self));
  pt_waveviewer_selection_set (PT_WAVEVIEWER_SELECTION (self->selection),
                               time_to_pixel (self, self->drag_fixed_pos),
                               time_to_pixel (self, self->drag_moving_pos));
}

static void
pt_waveviewer_drag_update (GtkGestureDrag *ctrl,
                           gdouble         offset_x,
                           gdouble         offset_y,
                           gpointer        user_data)
{
  PtWaveviewer *self = PT_WAVEVIEWER (user_data);
  gint64        pixel;

  if (self->peaks == NULL || self->peaks->len == 0)
    return;

  pixel = gtk_adjustment_get_value (self->adj) + self->drag_start_x + offset_x;
  self->drag_moving_pos = CLAMP (pixel_to_time (self, pixel), 0, self->duration);

  pt_waveviewer_selection_set (PT_WAVEVIEWER_SELECTION (self->selection),
                               time_to_pixel (self, self->drag_fixed_pos),
                               time_to_pixel (self, self->drag_moving_pos));
}

static void
pt_waveviewer_drag_end (GtkGestureDrag *ctrl,
                        gdouble         offset_x,
                        gdouble         offset_y,
                        gpointer        user_data)
{
  PtWaveviewer *self = PT_WAVEVIEWER (user_data);

  if (self->peaks == NULL || self->peaks->len == 0)
    return;

  /* Serves as in-drag-indicator. */
  self->drag_start_x = -1;

  update_selection (self);

  if (self->sel_start == self->sel_end)
    gtk_widget_set_cursor (GTK_WIDGET (self->cursor), NULL);
}

static gboolean
scrollbar_button_press_event_cb (GtkGestureClick *gesture,
                                 gint             n_press,
                                 gdouble          x,
                                 gdouble          y,
                                 gpointer         user_data)
{
  /* If user clicks on scrollbar don’t follow cursor anymore.
     Otherwise it would scroll immediately back again. */

  pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (user_data), FALSE);

  /* Propagate signal */
  return FALSE;
}

static gboolean
scrollbar_scroll_event_cb (GtkEventControllerScroll *ctrl,
                           gdouble                   delta_x,
                           gdouble                   delta_y,
                           gpointer                  user_data)
{
  /* If user scrolls on scrollbar don’t follow cursor anymore.
     Otherwise it would scroll immediately back again. */

  pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (user_data), FALSE);

  /* Propagate signal */
  return FALSE;
}

static gboolean
pt_waveviewer_focus (GtkWidget       *widget,
                     GtkDirectionType direction)
{
  PtWaveviewer *self = PT_WAVEVIEWER (widget);

  /* See API reference for gtk_widget_child_focus ():
     If moving into @direction would move focus outside of widget: return FALSE;
     If moving into @direction would steps into or stays inside widget: return TRUE */

  /* Don’t focus if empty */
  if (self->peaks == NULL || self->peaks->len == 0)
    return FALSE;

  if (gtk_widget_is_focus (widget))
    return FALSE;

  if (gtk_widget_get_can_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  return FALSE;
}

/**
 * pt_waveviewer_get_follow_cursor:
 * @self: the widget
 *
 * Get follow-cursor option.
 *
 * Returns: TRUE if cursor is followed, else FALSE
 *
 * Since: 1.5
 */
gboolean
pt_waveviewer_get_follow_cursor (PtWaveviewer *self)
{
  g_return_val_if_fail (PT_IS_WAVEVIEWER (self), FALSE);

  return self->follow_cursor;
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

  if (self->follow_cursor == follow)
    return;

  self->follow_cursor = follow;

  if (follow && self->peaks)
    scroll_to_cursor (self);

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_FOLLOW_CURSOR]);
}

static gboolean
cursor_is_visible (PtWaveviewer *self)
{
  gint cursor_pos, left, right;

  cursor_pos = time_to_pixel (self, self->playback_cursor);
  left = (gint) gtk_adjustment_get_value (self->adj);
  right = left + (gint) gtk_adjustment_get_page_size (self->adj);

  return (left <= cursor_pos && cursor_pos <= right);
}

static void
get_anchor_point (PtWaveviewer *self)
{
  /* Get an anchor point = zoom_pos, either cursor position or
     middle of view. The anchor point is relative to the view.
     Get time of that anchor point = zoom_time. */

  gint left;

  left = (gint) gtk_adjustment_get_value (self->adj);
  if (cursor_is_visible (self))
    {
      self->zoom_time = self->playback_cursor;
      self->zoom_pos = time_to_pixel (self, self->playback_cursor) - left;
    }
  else
    {
      self->zoom_pos = (gint) gtk_adjustment_get_page_size (self->adj) / 2;
      self->zoom_time = pixel_to_time (self, self->zoom_pos + left);
    }
}

static void
reset_selection (PtWaveviewer *self)
{
  if (self->has_selection == FALSE)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  self->has_selection = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_HAS_SELECTION]);

  if (self->sel_start > 0)
    {
      self->sel_start = 0;
      g_object_notify_by_pspec (G_OBJECT (self),
                                obj_properties[PROP_SELECTION_START]);
    }

  self->sel_end = 0;
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_SELECTION_END]);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
array_size_changed_cb (PtWaveloader *loader,
                       gpointer      user_data)
{
  PtWaveviewer *self = PT_WAVEVIEWER (user_data);
  gint          width;

  if (self->peaks == NULL || self->peaks->len == 0)
    {
      self->peaks_size = 0;
      self->px_per_sec = 0;
      self->duration = 0;
      pt_waveviewer_selection_set (
          PT_WAVEVIEWER_SELECTION (self->selection),
          0, 0);
      pt_waveviewer_cursor_render (
          PT_WAVEVIEWER_CURSOR (self->cursor),
          -1);
    }
  else
    {
      self->peaks_size = self->peaks->len;
      self->px_per_sec = self->pps;
      self->duration = calculate_duration (self);
    }

  width = self->peaks_size / 2;
  pt_waveviewer_scrollbox_set (PT_WAVEVIEWER_SCROLLBOX (self->scrollbox), width);

  pt_waveviewer_ruler_set_ruler (
      PT_WAVEVIEWER_RULER (self->ruler),
      self->peaks_size / 2,
      self->px_per_sec,
      self->duration);
}

static void
pt_waveviewer_set_pps (PtWaveviewer *self,
                       int           pps)
{
  GError *error = NULL;

  pps = CLAMP (pps, PPS_MIN, PPS_MAX);

  if (self->pps == pps)
    return;

  self->pps = pps;

  if (self->peaks->len == 0)
    return;

  get_anchor_point (self);
  if (!pt_waveloader_resize (self->loader, self->pps, &error))
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                        "MESSAGE", "%s", error->message);
      g_clear_error (&error);
      return;
    }

  array_size_changed_cb (NULL, self);
  gtk_adjustment_set_value (self->adj,
                            time_to_pixel (self, self->zoom_time) - self->zoom_pos);
  gtk_widget_queue_draw (self->waveform);
  render_cursor (self);
  if (self->has_selection)
    {
      pt_waveviewer_selection_set (PT_WAVEVIEWER_SELECTION (self->selection),
                                   time_to_pixel (self, self->sel_start),
                                   time_to_pixel (self, self->sel_end));
    }

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_PPS]);
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

  if (self->tick_handler != 0)
    {
      gtk_widget_remove_tick_callback (self->waveform,
                                       self->tick_handler);
      self->tick_handler = 0;
    }

  if (pt_waveloader_load_finish (loader, res, &error))
    {
      array_size_changed_cb (NULL, self);
      gtk_widget_queue_draw (self->waveform);
      render_cursor (self);
      g_task_return_boolean (task, TRUE);
    }
  else
    {
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
 * Returns: TRUE if successful, or FALSE with error set
 *
 * Since: 2.0
 */
gboolean
pt_waveviewer_load_wave_finish (PtWaveviewer *self,
                                GAsyncResult *result,
                                GError      **error)
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
 * @callback: (scope async) (closure user_data): a #GAsyncReadyCallback to call when the operation is complete
 * @user_data: user data for @callback
 *
 * Load wave form for the given URI. The initial resolution is set to
 * #PtWaveviewer:pps. While loading, a #PtWaveviewer::load-progress signal is
 * emitted. A previous waveform is discarded.
 *
 * Since: 2.0
 */
void
pt_waveviewer_load_wave_async (PtWaveviewer       *self,
                               gchar              *uri,
                               GCancellable       *cancel,
                               GAsyncReadyCallback callback,
                               gpointer            user_data)
{
  g_return_if_fail (PT_IS_WAVEVIEWER (self));
  g_return_if_fail (uri != NULL);

  GTask *task;
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

  g_object_set (self->loader, "uri", uri, NULL);
  self->px_per_sec = self->pps;
  if (self->tick_handler == 0)
    {
      self->tick_handler =
          gtk_widget_add_tick_callback (
              self->waveform,
              (GtkTickCallback) update_waveform_cb,
              self, NULL);
    }
  pt_waveloader_load_async (self->loader,
                            self->pps,
                            cancel,
                            (GAsyncReadyCallback) load_cb,
                            task);
}

static void
pt_waveviewer_finalize (GObject *object)
{
  PtWaveviewer *self = PT_WAVEVIEWER (object);

  g_clear_object (&self->arrows);
  g_clear_object (&self->loader);
  if (self->tick_handler != 0)
    {
      gtk_widget_remove_tick_callback (self->waveform,
                                       self->tick_handler);
      self->tick_handler = 0;
    }

  G_OBJECT_CLASS (pt_waveviewer_parent_class)->finalize (object);
}

static void
pt_waveviewer_dispose (GObject *object)
{
  PtWaveviewer *self = PT_WAVEVIEWER (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_WAVEVIEWER);

  G_OBJECT_CLASS (pt_waveviewer_parent_class)->dispose (object);
}

static void
pt_waveviewer_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PtWaveviewer *self = PT_WAVEVIEWER (object);

  switch (property_id)
    {
    case PROP_PLAYBACK_CURSOR:
      g_value_set_int64 (value, self->playback_cursor);
      break;
    case PROP_FOLLOW_CURSOR:
      g_value_set_boolean (value, self->follow_cursor);
      break;
    case PROP_FIXED_CURSOR:
      g_value_set_boolean (value, self->fixed_cursor);
      break;
    case PROP_SHOW_RULER:
      g_value_set_boolean (value, self->show_ruler);
      break;
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, self->has_selection);
      break;
    case PROP_SELECTION_START:
      g_value_set_int64 (value, self->sel_start);
      break;
    case PROP_SELECTION_END:
      g_value_set_int64 (value, self->sel_end);
      break;
    case PROP_PPS:
      g_value_set_int (value, self->pps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_waveviewer_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  PtWaveviewer *self = PT_WAVEVIEWER (object);

  switch (property_id)
    {
    case PROP_PLAYBACK_CURSOR:
      /* ignore if cursor’s value didn’t change */
      if (self->playback_cursor == g_value_get_int64 (value))
        break;
      self->playback_cursor = g_value_get_int64 (value);

      /* ignore if we’re not realized yet, widget is in construction */
      if (!gtk_widget_get_realized (GTK_WIDGET (self)))
        break;

      if (self->follow_cursor)
        scroll_to_cursor (self);

      render_cursor (self);
      break;
    case PROP_FOLLOW_CURSOR:
      pt_waveviewer_set_follow_cursor (self, g_value_get_boolean (value));
      break;
    case PROP_FIXED_CURSOR:
      self->fixed_cursor = g_value_get_boolean (value);
      break;
    case PROP_SHOW_RULER:
      self->show_ruler = g_value_get_boolean (value);
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer),
                                     self->show_ruler);
      break;
    case PROP_PPS:
      pt_waveviewer_set_pps (self, g_value_get_int (value));
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
  PtWaveviewer       *self = PT_WAVEVIEWER (object);
  GtkScrolledWindow  *scrolled_window = GTK_SCROLLED_WINDOW (self->scrolled_window);
  GtkWidget          *scrollbar;
  GtkGesture         *scrollbar_button_handler;
  GtkEventController *scrollbar_scroll_handler;

  /* Get Adjustment and Scrollbar from ScrolledWindow and connect signals */

  self->adj = gtk_scrolled_window_get_hadjustment (scrolled_window);

  g_signal_connect (self->adj,
                    "value_changed",
                    G_CALLBACK (adjustment_value_changed_cb),
                    self);

  scrollbar = gtk_scrolled_window_get_hscrollbar (scrolled_window);

  scrollbar_button_handler = gtk_gesture_click_new ();
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (scrollbar_button_handler), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (scrollbar_button_handler), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (scrollbar_button_handler), GTK_PHASE_CAPTURE);
  g_signal_connect (
      scrollbar_button_handler,
      "pressed",
      G_CALLBACK (scrollbar_button_press_event_cb),
      self);
  gtk_widget_add_controller (scrollbar, GTK_EVENT_CONTROLLER (scrollbar_button_handler));

  scrollbar_scroll_handler = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL | GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (scrollbar_scroll_handler), GTK_PHASE_CAPTURE);
  g_signal_connect (
      scrollbar_scroll_handler,
      "scroll",
      G_CALLBACK (scrollbar_scroll_event_cb),
      self);
  gtk_widget_add_controller (scrollbar, scrollbar_scroll_handler);
}

static GdkCursor *
get_resize_cursor (void)
{
  GdkCursor *result;

  result = gdk_cursor_new_from_name ("ew-resize", NULL);
  if (!result)
    result = gdk_cursor_new_from_name ("col-resize", NULL);

  return result;
}

static void
pt_waveviewer_init (PtWaveviewer *self)
{
  g_type_ensure (PT_TYPE_WAVEVIEWER_SCROLLBOX);
  g_type_ensure (PT_TYPE_WAVEVIEWER_RULER);
  g_type_ensure (PT_TYPE_WAVEVIEWER_WAVEFORM);
  g_type_ensure (PT_TYPE_WAVEVIEWER_SELECTION);
  g_type_ensure (PT_TYPE_WAVEVIEWER_CURSOR);

  gtk_widget_init_template (GTK_WIDGET (self));

  GtkCssProvider *provider;
  GFile          *css_file;

  self->peaks_size = 0;
  self->duration = 0;
  self->playback_cursor = 0;
  self->follow_cursor = TRUE;
  self->sel_start = 0;
  self->sel_end = 0;
  self->drag_start_x = -1;
  self->zoom_time = 0;
  self->zoom_pos = 0;
  self->arrows = get_resize_cursor ();
  self->loader = pt_waveloader_new (NULL);
  self->peaks = pt_waveloader_get_data (self->loader);
  self->tick_handler = 0;

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  pt_waveviewer_waveform_set (
      PT_WAVEVIEWER_WAVEFORM (self->waveform),
      self->peaks);

  css_file = g_file_new_for_uri ("resource:///xyz/parlatype/Parlatype/pt-waveviewer.css");
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_file (provider, css_file);
  gtk_style_context_add_provider_for_display (
      gdk_display_get_default (),
      GTK_STYLE_PROVIDER (provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_object_unref (css_file);
  g_object_unref (provider);

  /* Setup event handling */
  self->button = gtk_gesture_click_new ();
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (self->button), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->button), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->button), GTK_PHASE_CAPTURE);
  g_signal_connect (self->button,
                    "pressed",
                    G_CALLBACK (pt_waveviewer_button_press_event),
                    self);
  gtk_widget_add_controller (self->scrollbox, GTK_EVENT_CONTROLLER (self->button));

  self->motion_ctrl = gtk_event_controller_motion_new ();
  g_signal_connect (self->motion_ctrl,
                    "motion",
                    G_CALLBACK (pt_waveviewer_motion_event),
                    self);
  gtk_widget_add_controller (GTK_WIDGET (self->cursor), self->motion_ctrl);

  self->scroll_ctrl = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL | GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->scroll_ctrl), GTK_PHASE_CAPTURE);
  g_signal_connect (self->scroll_ctrl,
                    "scroll",
                    G_CALLBACK (pt_waveviewer_scroll_event),
                    self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->scroll_ctrl);

  self->key_ctrl = gtk_event_controller_key_new ();
  g_signal_connect (self->key_ctrl,
                    "key-pressed",
                    G_CALLBACK (pt_waveviewer_key_press_event),
                    self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->key_ctrl);

  GtkGesture *drag = gtk_gesture_drag_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (drag), GTK_PHASE_CAPTURE);
  g_signal_connect (drag,
                    "drag-begin",
                    G_CALLBACK (pt_waveviewer_drag_begin),
                    self);
  g_signal_connect (drag,
                    "drag-update",
                    G_CALLBACK (pt_waveviewer_drag_update),
                    self);
  g_signal_connect (drag,
                    "drag-end",
                    G_CALLBACK (pt_waveviewer_drag_end),
                    self);
  gtk_widget_add_controller (GTK_WIDGET (self->overlay), GTK_EVENT_CONTROLLER (drag));

  g_signal_connect (self->loader,
                    "progress",
                    G_CALLBACK (propagate_progress_cb),
                    self);

  g_signal_connect (self->loader,
                    "array-size-changed",
                    G_CALLBACK (array_size_changed_cb),
                    self);
}

static void
pt_waveviewer_class_init (PtWaveviewerClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = pt_waveviewer_set_property;
  gobject_class->get_property = pt_waveviewer_get_property;
  gobject_class->constructed = pt_waveviewer_constructed;
  gobject_class->dispose = pt_waveviewer_dispose;
  gobject_class->finalize = pt_waveviewer_finalize;

  widget_class->focus = pt_waveviewer_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/pt-waveviewer.ui");
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, scrollbox);
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, revealer);
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, ruler);
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, overlay);
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, waveform);
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, selection);
  gtk_widget_class_bind_template_child (widget_class, PtWaveviewer, cursor);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  /**
   * PtWaveviewer::load-progress:
   * @self: the waveviewer emitting the signal
   * @progress: the new progress state, ranging from 0.0 to 1.0
   *
   * Indicates progress on a scale from 0.0 to 1.0. The value 0.0 is not
   * emitted, the last signal (on success) is 1.0.
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
   * @self: the waveviewer emitting the signal
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
   * PtWaveviewer::selection-changed:
   * @self: the waveviewer emitting the signal
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
   * @self: the waveviewer emitting the signal
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
   * Current playback position in milliseconds. A value of -1 means an
   * invalid or unknown position that will not be rendered.
   */

  obj_properties[PROP_PLAYBACK_CURSOR] =
      g_param_spec_int64 (
          "playback-cursor", NULL, NULL,
          -1,
          G_MAXINT64,
          0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtWaveviewer:follow-cursor:
   *
   * If the widget follows the cursor, it scrolls automatically to the
   * cursor’s position. Note that the widget will change this property to
   * FALSE if the user scrolls the widget manually.
   */

  obj_properties[PROP_FOLLOW_CURSOR] =
      g_param_spec_boolean (
          "follow-cursor", NULL, NULL,
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
          "fixed-cursor", NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtWaveviewer:show-ruler:
   *
   * Whether the ruler is shown (TRUE) or not (FALSE).
   */

  obj_properties[PROP_SHOW_RULER] =
      g_param_spec_boolean (
          "show-ruler", NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtWaveviewer:has-selection:
   *
   * Whether something is selected (TRUE) or not (FALSE).
   */

  obj_properties[PROP_HAS_SELECTION] =
      g_param_spec_boolean (
          "has-selection", NULL, NULL,
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
          "selection-start", NULL, NULL,
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
          "selection-end", NULL, NULL,
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
          "pps", NULL, NULL,
          PPS_MIN,
          PPS_MAX,
          100,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

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
