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
 * pt-waveviewer-selection
 * Internal widget that draws a selection for PtWaveviewer.
 *
 * PtWaveviewerSelection is part of a GtkOverlay stack, from bottom to top:
 * - PtWaveviewerWaveform
 * - PtWaveviewerSelection
 * - PtWaveviewerCursor
 * - PtWaveviewerFocus
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
#include "pt-waveviewer.h"
#include "pt-waveviewer-selection.h"


struct _PtWaveviewerSelectionPrivate {
	GtkAdjustment *adj;	/* the parent PtWaveviewer’s adjustment */

	GdkRGBA	selection_color;
	gint    start;
	gint    end;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewerSelection, pt_waveviewer_selection, GTK_TYPE_DRAWING_AREA);


static gboolean
pt_waveviewer_selection_draw (GtkWidget *widget,
                              cairo_t   *cr)
{
	PtWaveviewerSelection *self = (PtWaveviewerSelection *) widget;

	gint height;
	gint width;
	gint offset;
	gint left, right;

	height = gtk_widget_get_allocated_height (widget);
	width = gtk_widget_get_allocated_width (widget);

	offset = (gint) gtk_adjustment_get_value (self->priv->adj);
	left = CLAMP (self->priv->start - offset, 0, width);
	right = CLAMP (self->priv->end - offset, 0, width);

	if (left == right)
		return FALSE;

	gdk_cairo_set_source_rgba (cr, &self->priv->selection_color);
	cairo_rectangle (cr, left, 0, right - left, height);
	cairo_fill (cr);

	return FALSE;
}

static void
update_cached_style_values (PtWaveviewerSelection *self)
{
	/* Update color */

	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_get_color (context, &self->priv->selection_color);
}

static void
pt_waveviewer_selection_state_flags_changed (GtkWidget     *widget,
                                             GtkStateFlags  flags)
{
	update_cached_style_values (PT_WAVEVIEWER_SELECTION (widget));
	GTK_WIDGET_CLASS (pt_waveviewer_selection_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_waveviewer_selection_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (pt_waveviewer_selection_parent_class)->realize (widget);
	update_cached_style_values (PT_WAVEVIEWER_SELECTION (widget));
}

static void
adj_value_changed (GtkAdjustment *adj,
                   gpointer       data)
{
	PtWaveviewerSelection *self = PT_WAVEVIEWER_SELECTION (data);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_selection_hierarchy_changed (GtkWidget *widget,
                                           GtkWidget *old_parent)
{
	/* When added as a child to the PtWaveviewer, get its horizontal
	 * GtkAdjustment */

	PtWaveviewerSelection *self = PT_WAVEVIEWER_SELECTION (widget);
	GtkWidget *parent = NULL;

	if (self->priv->adj)
		return;

	parent = gtk_widget_get_ancestor (widget, GTK_TYPE_SCROLLED_WINDOW);

	if (!parent)
		return;

	self->priv->adj = gtk_scrolled_window_get_hadjustment (
					GTK_SCROLLED_WINDOW (parent));
	g_signal_connect (self->priv->adj, "value-changed",
	                  G_CALLBACK (adj_value_changed), self);
}

void
pt_waveviewer_selection_set (PtWaveviewerSelection *self,
                             gint                   start,
                             gint                   end)
{
	self->priv->start = start;
	self->priv->end = end;
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_selection_init (PtWaveviewerSelection *self)
{
	self->priv = pt_waveviewer_selection_get_instance_private (self);

	GtkStyleContext *context;

	self->priv->start = 0;
	self->priv->end = 0;
	self->priv->adj = NULL;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, "selection");
}

static void
pt_waveviewer_selection_class_init (PtWaveviewerSelectionClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->draw                = pt_waveviewer_selection_draw;
	widget_class->hierarchy_changed   = pt_waveviewer_selection_hierarchy_changed;
	widget_class->realize             = pt_waveviewer_selection_realize;
	widget_class->state_flags_changed = pt_waveviewer_selection_state_flags_changed;
}

GtkWidget *
pt_waveviewer_selection_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_SELECTION, NULL));
}
