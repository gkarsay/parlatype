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
#include "pt-waveviewer.h"
#include "pt-waveviewer-waveform.h"


struct _PtWaveviewerWaveformPrivate {
	/* Wavedata */
	GArray   *peaks;

	GtkAdjustment *adj;	/* the parent PtWaveviewer’s adjustment */

	/* Rendering */
	GdkRGBA   wave_color;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewerWaveform, pt_waveviewer_waveform, GTK_TYPE_DRAWING_AREA);


static gint64
pixel_to_array (PtWaveviewerWaveform *self,
                gint64                pixel)
{
	/* Convert a position in the drawing area to an index in the peaks array.
	   The returned index is the peak’s min value, +1 is the max value.
	   If the array would be exceeded, return -1 */

	gint64 result;

	result = pixel * 2;
	if (result + 1 >= self->priv->peaks->len)
		result = -1;

	return result;
}

static void
pt_waveviewer_waveform_snapshot (GtkWidget  *widget,
				 GtkSnapshot *snapshot)
{
	PtWaveviewerWaveform *self = (PtWaveviewerWaveform *) widget;

	GArray *peaks = self->priv->peaks;
	GtkStyleContext *context;
	gint pixel;
	gint array;
	gdouble min, max;
	gint width, height, offset;
	gint half, middle;

	context = gtk_widget_get_style_context (GTK_WIDGET (widget));
	height = gtk_widget_get_allocated_height (GTK_WIDGET (widget));
	width = gtk_widget_get_allocated_width (GTK_WIDGET (widget));

	gtk_style_context_get_color (context, &self->priv->wave_color);
	gtk_snapshot_render_background (snapshot, context,
                               0, 0,
                               width, height);
	if (peaks == NULL || peaks->len == 0)
		return;

	/* paint waveform */
	offset = (gint) gtk_adjustment_get_value (self->priv->adj);
	half = height / 2 - 1;
	middle = height / 2;
	for (pixel = 0; pixel <= width; pixel += 1) {
		array = pixel_to_array (self, pixel + offset);
		if (array == -1)
			break;
		min = (middle + half * g_array_index (peaks, float, array) * -1);
		max = (middle - half * g_array_index (peaks, float, array + 1));
		gtk_snapshot_append_color (snapshot, &self->priv->wave_color,
					   &GRAPHENE_RECT_INIT(pixel, max, 1, min-max));
	}
}

static void
update_cached_style_values (PtWaveviewerWaveform *self)
{
	/* Update color */

	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_get_color (context, &self->priv->wave_color);
}

static void
pt_waveviewer_waveform_state_flags_changed (GtkWidget     *widget,
                                            GtkStateFlags  flags)
{
	update_cached_style_values (PT_WAVEVIEWER_WAVEFORM (widget));
	GTK_WIDGET_CLASS (pt_waveviewer_waveform_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_waveviewer_waveform_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (pt_waveviewer_waveform_parent_class)->realize (widget);
	update_cached_style_values (PT_WAVEVIEWER_WAVEFORM (widget));
}

void
pt_waveviewer_waveform_set (PtWaveviewerWaveform *self,
                            GArray               *peaks)
{
	self->priv->peaks = peaks;
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

	if (self->priv->adj)
		return;

	/* Get parent’s GtkAdjustment */
	GtkWidget *parent = NULL;
	parent = gtk_widget_get_ancestor (widget, GTK_TYPE_SCROLLED_WINDOW);
	if (parent) {
		self->priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (parent));
		g_signal_connect (self->priv->adj, "value-changed", G_CALLBACK (adj_value_changed), self);
	}
}

static void
pt_waveviewer_waveform_init (PtWaveviewerWaveform *self)
{
	self->priv = pt_waveviewer_waveform_get_instance_private (self);

	GtkStyleContext *context;

	self->priv->peaks = NULL;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, "view");
}

static void
pt_waveviewer_waveform_class_init (PtWaveviewerWaveformClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->realize             = pt_waveviewer_waveform_realize;
	widget_class->root                = pt_waveviewer_waveform_root;
	widget_class->snapshot            = pt_waveviewer_waveform_snapshot;
	widget_class->state_flags_changed = pt_waveviewer_waveform_state_flags_changed;
}

GtkWidget *
pt_waveviewer_waveform_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_WAVEFORM, NULL));
}
