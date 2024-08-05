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
 * pt-waveviewer-waveform
 * Internal widget that draws the waveform for PtWaveviewer.
 *
 * PtWaveviewerCursor is part of a GtkOverlay stack, from bottom to top:
 * - PtWaveviewerWaveform
 * - PtWaveviewerSelection
 * - PtWaveviewerCursor
 *
 * When it's added to PtWaveviewer, it gets the horizontal GtkAdjustment from
 * its parent (listening for the hierarchy-changed signal).
 *
 * pt_waveviewer_waveform_set() is used to pass an array with data to the
 * widget.
 *
 * It listens to changes of the parent's horizontal GtkAdjustment and redraws
 * itself (scroll movements, size changes).
 *
 * The widget listens to the style-updated signal and the state-flags-changed
 * signal to update its color.
 *
 * The widget has the GTK_STYLE_CLASS_VIEW to paint the waveform.
 */

#include "config.h"

#include "pt-waveviewer-waveform.h"

#include "pt-waveviewer.h"

struct _PtWaveviewerWaveform
{
  GtkWidget parent;

  /* Wavedata */
  GArray *peaks;

  GtkAdjustment *adj; /* the parent PtWaveviewer’s adjustment */

  /* Rendering */
  GdkRGBA wave_color;
};

G_DEFINE_TYPE (PtWaveviewerWaveform, pt_waveviewer_waveform, GTK_TYPE_WIDGET);

static gint64
pixel_to_array (PtWaveviewerWaveform *self,
                gint64                pixel)
{
  /* Convert a position in the drawing area to an index in the peaks array.
     The returned index is the peak’s min value, +1 is the max value.
     If the array would be exceeded, return -1 */

  gint64 result;

  result = pixel * 2;
  if (result + 1 >= self->peaks->len)
    result = -1;

  return result;
}

static void
pt_waveviewer_waveform_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  PtWaveviewerWaveform *self = (PtWaveviewerWaveform *) widget;

  GArray *peaks = self->peaks;
  gint    pixel;
  gint    array;
  gdouble min, max;
  gint    width, height, offset;
  gint    half, middle;

  if (peaks == NULL || peaks->len == 0)
    return;

  height = gtk_widget_get_height (widget);
  width = gtk_widget_get_width (widget);
  gtk_widget_get_color (widget, &self->wave_color);

  /* paint waveform */
  offset = (gint) gtk_adjustment_get_value (self->adj);
  half = height / 2 - 1;
  middle = height / 2;
  for (pixel = 0; pixel <= width; pixel += 1)
    {
      array = pixel_to_array (self, pixel + offset);
      if (array == -1)
        break;
      min = (middle + half * g_array_index (peaks, float, array) * -1);
      max = (middle - half * g_array_index (peaks, float, array + 1));
      gtk_snapshot_append_color (snapshot, &self->wave_color,
                                 &GRAPHENE_RECT_INIT (pixel, max, 1, min - max));
    }
}

static void
pt_waveviewer_waveform_state_flags_changed (GtkWidget    *widget,
                                            GtkStateFlags flags)
{
  PtWaveviewerWaveform *self = (PtWaveviewerWaveform *) widget;

  gtk_widget_get_color (widget, &self->wave_color);
  gtk_widget_queue_draw (widget);
  GTK_WIDGET_CLASS (pt_waveviewer_waveform_parent_class)->state_flags_changed (widget, flags);
}

void
pt_waveviewer_waveform_set (PtWaveviewerWaveform *self,
                            GArray               *peaks)
{
  self->peaks = peaks;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
adj_value_changed (GtkAdjustment *adj,
                   gpointer       data)
{
  PtWaveviewerWaveform *self = PT_WAVEVIEWER_WAVEFORM (data);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_waveform_root (GtkWidget *widget)
{
  PtWaveviewerWaveform *self = PT_WAVEVIEWER_WAVEFORM (widget);

  if (self->adj)
    return;

  /* Get parent’s GtkAdjustment */
  GtkWidget *parent = NULL;
  parent = gtk_widget_get_ancestor (widget, GTK_TYPE_SCROLLED_WINDOW);
  if (parent)
    {
      self->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (parent));
      g_signal_connect (self->adj, "value-changed", G_CALLBACK (adj_value_changed), self);
    }
}

static void
pt_waveviewer_waveform_init (PtWaveviewerWaveform *self)
{
  self->peaks = NULL;
  gtk_widget_add_css_class (GTK_WIDGET (self), "view");
  gtk_widget_get_color (GTK_WIDGET (self), &self->wave_color);
}

static void
pt_waveviewer_waveform_class_init (PtWaveviewerWaveformClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->root = pt_waveviewer_waveform_root;
  widget_class->snapshot = pt_waveviewer_waveform_snapshot;
  widget_class->state_flags_changed = pt_waveviewer_waveform_state_flags_changed;
}

GtkWidget *
pt_waveviewer_waveform_new (void)
{
  return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_WAVEFORM, NULL));
}
