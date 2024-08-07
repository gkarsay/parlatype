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
 * pt-waveviewer-selection
 * Internal widget that draws a selection for PtWaveviewer.
 *
 * PtWaveviewerSelection is part of a GtkOverlay stack, from bottom to top:
 * - PtWaveviewerWaveform
 * - PtWaveviewerSelection
 * - PtWaveviewerCursor
 *
 * When it's added to PtWaveviewer, it gets the horizontal GtkAdjustment from
 * its parent (listening for the hierarchy-changed signal).
 *
 * pt_waveviewer_selection_set() is used to set (or remove) a selection. The
 * parameters @start and @end are the first and the last pixel of the selection
 * of a fully plotted waveform. The widget knows from the parent's GtkAdjustment
 * which portion of the waveform is currently shown and draws a selection box
 * if it is currently visible. To remove a selection set @start and @end to
 * the same value, e.g. zero.
 *
 * The widget has a style class "selection" that can be used to set its color
 * via CSS.
 */

#include "config.h"

#include "pt-waveviewer-selection.h"

#include "pt-waveviewer.h"

struct _PtWaveviewerSelection
{
  GtkWidget parent;

  GtkAdjustment *adj; /* the parent PtWaveviewer’s adjustment */

  GdkRGBA selection_color;
  gint    start;
  gint    end;
};

G_DEFINE_TYPE (PtWaveviewerSelection, pt_waveviewer_selection, GTK_TYPE_WIDGET);

static void
pt_waveviewer_selection_snapshot (GtkWidget   *widget,
                                  GtkSnapshot *snapshot)
{
  PtWaveviewerSelection *self = (PtWaveviewerSelection *) widget;

  gint height;
  gint width;
  gint offset;
  gint left, right;

  height = gtk_widget_get_height (widget);
  width = gtk_widget_get_width (widget);

  offset = (gint) gtk_adjustment_get_value (self->adj);
  left = CLAMP (self->start - offset, 0, width);
  right = CLAMP (self->end - offset, 0, width);

  if (left == right)
    return;

  gtk_snapshot_append_color (snapshot, &self->selection_color,
                             &GRAPHENE_RECT_INIT (left, 0, right - left, height));
}

static void
pt_waveviewer_selection_css_changed (GtkWidget         *widget,
                                     GtkCssStyleChange *change)
{
  /* Sometimes the color was not picked up if called early in init,
   * so this is the first color assignment. */

  PtWaveviewerSelection *self = PT_WAVEVIEWER_SELECTION (widget);
  gtk_widget_get_color (widget, &self->selection_color);
  GTK_WIDGET_CLASS (pt_waveviewer_selection_parent_class)->css_changed (widget, change);
}

static void
pt_waveviewer_selection_state_flags_changed (GtkWidget    *widget,
                                             GtkStateFlags flags)
{
  PtWaveviewerSelection *self = PT_WAVEVIEWER_SELECTION (widget);

  gtk_widget_get_color (widget, &self->selection_color);
  gtk_widget_queue_draw (widget);
  GTK_WIDGET_CLASS (pt_waveviewer_selection_parent_class)->state_flags_changed (widget, flags);
}

static void
adj_value_changed (GtkAdjustment *adj,
                   gpointer       data)
{
  PtWaveviewerSelection *self = PT_WAVEVIEWER_SELECTION (data);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_selection_root (GtkWidget *widget)
{
  /* When added as a child to the PtWaveviewer, get its horizontal
   * GtkAdjustment */

  PtWaveviewerSelection *self = PT_WAVEVIEWER_SELECTION (widget);
  GtkWidget             *parent = NULL;

  if (self->adj)
    return;

  parent = gtk_widget_get_ancestor (widget, GTK_TYPE_SCROLLED_WINDOW);

  if (!parent)
    return;

  self->adj = gtk_scrolled_window_get_hadjustment (
      GTK_SCROLLED_WINDOW (parent));
  g_signal_connect (self->adj, "value-changed",
                    G_CALLBACK (adj_value_changed), self);
}

void
pt_waveviewer_selection_set (PtWaveviewerSelection *self,
                             gint                   start,
                             gint                   end)
{
  if (end < start)
    {
      self->start = end;
      self->end = start;
    }
  else
    {
      self->start = start;
      self->end = end;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_selection_init (PtWaveviewerSelection *self)
{
  self->start = 0;
  self->end = 0;
  self->adj = NULL;

  gtk_widget_add_css_class (GTK_WIDGET (self), "selection");
}

static void
pt_waveviewer_selection_class_init (PtWaveviewerSelectionClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->css_changed = pt_waveviewer_selection_css_changed;
  widget_class->root = pt_waveviewer_selection_root;
  widget_class->snapshot = pt_waveviewer_selection_snapshot;
  widget_class->state_flags_changed = pt_waveviewer_selection_state_flags_changed;
}

GtkWidget *
pt_waveviewer_selection_new (void)
{
  return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_SELECTION, NULL));
}
