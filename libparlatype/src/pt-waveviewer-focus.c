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

/* PtWaveviewerFocus is a GtkDrawingArea. It is part of a GtkOverlay stack,
   on the very top. It renders only a focus line on the visible rectangle of
   the PtWaveviewer parent. */


#include "config.h"
#include "pt-waveviewer.h"
#include "pt-waveviewer-focus.h"


struct _PtWaveviewerFocusPrivate {
	gboolean         focus; /* whether to render focus or not */
	GtkAdjustment   *adj;	/* the parent PtWaveviewer’s adjustment */
};


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewerFocus, pt_waveviewer_focus, GTK_TYPE_DRAWING_AREA);


static gboolean
pt_waveviewer_focus_draw (GtkWidget *widget,
                          cairo_t   *cr)
{
	PtWaveviewerFocus *self = (PtWaveviewerFocus *) widget;

	if (!self->priv->focus || !self->priv->adj)
		return FALSE;

	GtkStyleContext *context;
	gint left, height, width;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	height = gtk_widget_get_allocated_height (widget);
	width = (gint) gtk_adjustment_get_page_size (self->priv->adj);
	left = (gint) gtk_adjustment_get_value (self->priv->adj);

	gtk_render_focus (context, cr, left, 0, width, height);
	return FALSE;
}

static void
adj_value_changed (GtkAdjustment *adj,
                   gpointer       data)
{
	PtWaveviewerFocus *self = PT_WAVEVIEWER_FOCUS (data);

	if (self->priv->focus)
		gtk_widget_queue_draw (GTK_WIDGET (data));
}

static void
pt_waveviewer_focus_hierarchy_changed (GtkWidget *widget,
                                       GtkWidget *old_parent)
{
	PtWaveviewerFocus *self = PT_WAVEVIEWER_FOCUS (widget);

	if (self->priv->adj)
		return;

	/* Get parent’s GtkAdjustment */
	GtkWidget *parent = NULL;
	parent = gtk_widget_get_ancestor (widget, PT_TYPE_WAVEVIEWER);
	if (parent) {
		self->priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (parent));
		g_signal_connect (self->priv->adj, "value-changed", G_CALLBACK (adj_value_changed), self);
	}
}

void
pt_waveviewer_focus_set (PtWaveviewerFocus *self,
                         gboolean           focus)
{
	if (self->priv->focus == focus)
		return;
	self->priv->focus = focus;
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
pt_waveviewer_focus_init (PtWaveviewerFocus *self)
{
	self->priv = pt_waveviewer_focus_get_instance_private (self);

	self->priv->focus = FALSE;
	self->priv->adj = NULL;

	gtk_widget_set_name (GTK_WIDGET (self), "focus");
	gtk_widget_set_events (GTK_WIDGET (self), GDK_ALL_EVENTS_MASK);
}

static void
pt_waveviewer_focus_class_init (PtWaveviewerFocusClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->hierarchy_changed = pt_waveviewer_focus_hierarchy_changed;
	widget_class->draw = pt_waveviewer_focus_draw;
}

GtkWidget *
pt_waveviewer_focus_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_FOCUS, NULL));
}
