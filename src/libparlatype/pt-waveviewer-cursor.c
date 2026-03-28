/* Copyright 2017 Gabor Karsay <gabor.karsay@gmx.at>
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
  GtkWidget parent;
  GdkRGBA   cursor_color;
  gint      position;
};

G_DEFINE_TYPE (PtWaveviewerCursor, pt_waveviewer_cursor, GTK_TYPE_WIDGET);

static void
pt_waveviewer_cursor_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  PtWaveviewerCursor *self = (PtWaveviewerCursor *) widget;

  if (self->position == -1)
    return;

  gdouble         height = gtk_widget_get_height (GTK_WIDGET (widget));
  GskPathBuilder *builder;
  GskPath        *path;
  GskStroke      *stroke;

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, self->position, MARKER_BOX_H);
  gsk_path_builder_line_to (builder, self->position - MARKER_BOX_W / 2, 0);
  gsk_path_builder_line_to (builder, self->position + MARKER_BOX_W / 2, 0);
  gsk_path_builder_close (builder);
  gsk_path_builder_line_to (builder, self->position, height);

  path = gsk_path_builder_free_to_path (builder);
  stroke = gsk_stroke_new (2.0);
  gtk_snapshot_append_stroke (snapshot, path, stroke, &self->cursor_color);
  gtk_snapshot_append_fill (snapshot, path, GSK_FILL_RULE_WINDING, &self->cursor_color);
}

static void
pt_waveviewer_cursor_css_changed (GtkWidget         *widget,
                                  GtkCssStyleChange *change)
{
  PtWaveviewerCursor *self = PT_WAVEVIEWER_CURSOR (widget);
  gtk_widget_get_color (widget, &self->cursor_color);
  GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->css_changed (widget, change);
}

static void
pt_waveviewer_cursor_state_flags_changed (GtkWidget    *widget,
                                          GtkStateFlags flags)
{
  PtWaveviewerCursor *self = PT_WAVEVIEWER_CURSOR (widget);

  gtk_widget_get_color (widget, &self->cursor_color);
  gtk_widget_queue_draw (widget);
  GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->state_flags_changed (widget, flags);
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
  self->position = -1;
  gtk_widget_add_css_class (GTK_WIDGET (self), "cursor");
}

static void
pt_waveviewer_cursor_class_init (PtWaveviewerCursorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->css_changed = pt_waveviewer_cursor_css_changed;
  widget_class->snapshot = pt_waveviewer_cursor_snapshot;
  widget_class->state_flags_changed = pt_waveviewer_cursor_state_flags_changed;
}

GtkWidget *
pt_waveviewer_cursor_new (void)
{
  return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_CURSOR, NULL));
}
