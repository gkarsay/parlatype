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
 * pt-waveviewer-cursor
 * Internal widget that draws a cursor for PtWaveviewer.
 *
 * PtWaveviewerCursor is part of a GtkOverlay stack, from bottom to top:
 * - PtWaveviewerWaveform
 * - PtWaveviewerSelection
 * - PtWaveviewerCursor
 *
 * pt_waveviewer_cursor_render() is used to render the cursor. The parameter
 * @position is relative to the viewport. That means that the caller has to
 * compute the position from the absolute position (in terms of a fully plotted
 * waveform) minus the left position of the viewport (the value of the horizontal
 * GtkAdjustment).
 *
 * A value of -1 hides the cursor.
 *
 *
 * The cursor itself is cached and updated when the style changes, e.g. color
 * (style-updated signal), state flags change, e.g. window in foreground or
 * background (state-flags-changed signal) or the widget's vertical size
 * changes (size-allocate signal).
 *
 * The widget has a style class "cursor" that can be used to set its color
 * via CSS. Horizontal size and shape are immutable.
 */

#include "config.h"

#include "pt-waveviewer-cursor.h"

#include "pt-waveviewer.h"

#define MARKER_BOX_W 10
#define MARKER_BOX_H 8

struct _PtWaveviewerCursor
{
  GtkDrawingArea parent;

  cairo_surface_t *cursor;
  GdkRGBA          cursor_color;
  gint             position;
};

G_DEFINE_TYPE (PtWaveviewerCursor, pt_waveviewer_cursor, GTK_TYPE_DRAWING_AREA);

static void
pt_waveviewer_cursor_draw (GtkDrawingArea *widget,
                           cairo_t        *cr,
                           int             content_width,
                           int             content_height,
                           gpointer        user_data)
{
  PtWaveviewerCursor *self = (PtWaveviewerCursor *) widget;

  if (self->position == -1)
    return;

  cairo_set_source_surface (cr, self->cursor,
                            self->position - MARKER_BOX_W / 2, 0);
  cairo_paint (cr);
}

static void
cache_cursor (PtWaveviewerCursor *self)
{
  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  if (self->cursor)
    cairo_surface_destroy (self->cursor);

  cairo_t    *cr;
  gint        height = gtk_widget_get_height (GTK_WIDGET (self));
  GtkNative  *native;
  GdkSurface *surface;

  native = gtk_widget_get_native (GTK_WIDGET (self));
  surface = gtk_native_get_surface (native);
  self->cursor = gdk_surface_create_similar_surface (surface,
                                                     CAIRO_CONTENT_COLOR_ALPHA,
                                                     MARKER_BOX_W,
                                                     height);
  cr = cairo_create (self->cursor);

  gdk_cairo_set_source_rgba (cr, &self->cursor_color);

  cairo_move_to (cr, 0 + MARKER_BOX_W / 2, height);
  cairo_line_to (cr, 0 + MARKER_BOX_W / 2, 0);
  cairo_stroke (cr);
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, 0 + MARKER_BOX_W, 0);
  cairo_line_to (cr, 0 + MARKER_BOX_W / 2, 0 + MARKER_BOX_H);
  cairo_line_to (cr, 0, 0);
  cairo_fill (cr);

  cairo_destroy (cr);
}

static void
pt_waveviewer_cursor_size_allocate (GtkWidget *widget,
                                    gint       width,
                                    gint       height,
                                    gint       baseline)
{
  GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->size_allocate (widget, width, height, baseline);

  /* If widget changed vertical size, cursor’s size has to be adjusted */
  cache_cursor (PT_WAVEVIEWER_CURSOR (widget));
}

static void
update_cached_style_values (PtWaveviewerCursor *self)
{
  /* Update color */

  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_get_color (context, &self->cursor_color);

  cache_cursor (self);
}

static void
pt_waveviewer_cursor_state_flags_changed (GtkWidget    *widget,
                                          GtkStateFlags flags)
{
  update_cached_style_values (PT_WAVEVIEWER_CURSOR (widget));
  GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_waveviewer_cursor_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->realize (widget);
  update_cached_style_values (PT_WAVEVIEWER_CURSOR (widget));
}

void
pt_waveviewer_cursor_render (PtWaveviewerCursor *self,
                             gint                position)
{
  gint width;

  width = gtk_widget_get_width (GTK_WIDGET (self));
  position = CLAMP (position, -1, width + MARKER_BOX_W);

  if (self->position == position)
    return;

  self->position = position;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_cursor_init (PtWaveviewerCursor *self)
{
  self->cursor = NULL;
  self->position = -1;
  gtk_widget_add_css_class (GTK_WIDGET (self), "cursor");

  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self), pt_waveviewer_cursor_draw, NULL, NULL);
}

static void
pt_waveviewer_cursor_finalize (GObject *object)
{
  PtWaveviewerCursor *self = PT_WAVEVIEWER_CURSOR (object);

  if (self->cursor)
    cairo_surface_destroy (self->cursor);

  G_OBJECT_CLASS (pt_waveviewer_cursor_parent_class)->finalize (object);
}

static void
pt_waveviewer_cursor_class_init (PtWaveviewerCursorClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize = pt_waveviewer_cursor_finalize;

  widget_class->realize = pt_waveviewer_cursor_realize;
  widget_class->size_allocate = pt_waveviewer_cursor_size_allocate;
  widget_class->state_flags_changed = pt_waveviewer_cursor_state_flags_changed;
}

GtkWidget *
pt_waveviewer_cursor_new (void)
{
  return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_CURSOR, NULL));
}
