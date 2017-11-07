/* Copyright (C) 2017 Gabor Karsay <gabor.karsay@gmx.at>
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

/* PtWaveviewerSelection is a GtkDrawingArea. It is part of a GtkOverlay stack.
   It renders only the selection. */


#include "config.h"
#include "pt-waveviewer-selection.h"


struct _PtWaveviewerSelectionPrivate {
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

	height = gtk_widget_get_allocated_height (widget);
	width = gtk_widget_get_allocated_width (widget);

	/* clear everything */
	cairo_set_source_rgba (cr, 0, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	/* paint selection */
	if (self->priv->start != self->priv->end) {
		gdk_cairo_set_source_rgba (cr, &self->priv->selection_color);
		cairo_rectangle (cr, self->priv->start, 0, self->priv->end - self->priv->start, height);
		cairo_fill (cr);
	}

	return FALSE;
}

static void
draw_selection (PtWaveviewerSelection *self)
{
	gint height;

	height = gtk_widget_get_allocated_height (GTK_WIDGET (self));
	gtk_widget_queue_draw_area (GTK_WIDGET (self),
				    self->priv->start, 0,
				    self->priv->end - self->priv->start, height);
}

static void
update_cached_style_values (PtWaveviewerSelection *self)
{
	/* Update color */

	GtkStyleContext *context;
	GtkStateFlags    state;
	GdkWindow       *window = NULL;

	window = gtk_widget_get_parent_window (GTK_WIDGET (self));
	if (!window)
		return;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	state = gtk_style_context_get_state (context);
	gtk_style_context_get_color (context, state, &self->priv->selection_color);
}

static void
pt_waveviewer_selection_state_flags_changed (GtkWidget	   *widget,
					     GtkStateFlags  flags)
{
	update_cached_style_values (PT_WAVEVIEWER_SELECTION (widget));
	GTK_WIDGET_CLASS (pt_waveviewer_selection_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_waveviewer_selection_style_updated (GtkWidget *widget)
{
	PtWaveviewerSelection *self = (PtWaveviewerSelection *) widget;

	GTK_WIDGET_CLASS (pt_waveviewer_selection_parent_class)->style_updated (widget);

	update_cached_style_values (self);
	draw_selection (self);
}

static void
pt_waveviewer_selection_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (pt_waveviewer_selection_parent_class)->realize (widget);
	update_cached_style_values (PT_WAVEVIEWER_SELECTION (widget));
}

void
pt_waveviewer_selection_set (PtWaveviewerSelection *self,
			     gint                   start,
			     gint                   end)
{
	gint x1, x2, height;

	/* Case: no change */
	if (self->priv->start == start && self->priv->end == end)
		return;

	/* Case: nothing selected before, draw selection only */
	if (self->priv->start == self->priv->end) {
		self->priv->start = start;
		self->priv->end = end;
		draw_selection (self);
		return;
	}

	/* Case: start and end changed, draw old and new selection */
	if (self->priv->start != start && self->priv->end != end) {
		draw_selection (self);
		self->priv->start = start;
		self->priv->end = end;
		draw_selection (self);
		return;
	}

	if (self->priv->start == start) {
		/* Case: start unchanged, draw only difference at end */
		if (self->priv->end < end) {
			x1 = self->priv->end;
			x2 = end - self->priv->end;
		} else {
			x1 = end;
			x2 = self->priv->end - end;
		}
	} else {
		/* Case: end unchanged, draw only difference at start */
		if (self->priv->start < start) {
			x1 = self->priv->start;
			x2 = start - self->priv->start;
		} else {
			x1 = start;
			x2 = self->priv->start - start;
		}
	}

	self->priv->start = start;
	self->priv->end = end;
	height = gtk_widget_get_allocated_height (GTK_WIDGET (self));
	gtk_widget_queue_draw_area (GTK_WIDGET (self),
				    x1, 0,
				    x2, height);
}

static void
pt_waveviewer_selection_init (PtWaveviewerSelection *self)
{
	self->priv = pt_waveviewer_selection_get_instance_private (self);

	GtkStyleContext *context;

	self->priv->start = 0;
	self->priv->end = 0;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, "selection");

	gtk_widget_set_events (GTK_WIDGET (self), GDK_ALL_EVENTS_MASK);
}

static void
pt_waveviewer_selection_class_init (PtWaveviewerSelectionClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->draw                = pt_waveviewer_selection_draw;
	widget_class->realize             = pt_waveviewer_selection_realize;
	widget_class->state_flags_changed = pt_waveviewer_selection_state_flags_changed;
	widget_class->style_updated       = pt_waveviewer_selection_style_updated;
}

GtkWidget *
pt_waveviewer_selection_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_SELECTION, NULL));
}
