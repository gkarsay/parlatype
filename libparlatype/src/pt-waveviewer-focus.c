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
 * pt-waveviewer-focus
 * Internal widget that draws a focus indicator for PtWaveviewer.
 *
 * PtWaveviewerFocus is part of a GtkOverlay stack, from bottom to top:
 * - PtWaveviewerWaveform
 * - PtWaveviewerSelection
 * - PtWaveviewerCursor
 * - PtWaveviewerFocus
 *
 * pt_waveviewer_focus_set() is used to set (or remove) the focus indicator.
 * It is drawn around the currently visible part of the waveform.
 */


#include "config.h"
#include "pt-waveviewer.h"
#include "pt-waveviewer-focus.h"


struct _PtWaveviewerFocusPrivate {
	gboolean focus;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewerFocus, pt_waveviewer_focus, GTK_TYPE_DRAWING_AREA);


static void
pt_waveviewer_focus_draw (GtkDrawingArea *widget,
                          cairo_t        *cr,
                          int             content_width,
                          int             content_height,
                          gpointer        user_data)
{
	PtWaveviewerFocus *self = (PtWaveviewerFocus *) widget;

	if (!self->priv->focus)
		return;

	GtkStyleContext *context;
	gint height, width;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	height = gtk_widget_get_allocated_height (GTK_WIDGET (widget));
	width = gtk_widget_get_allocated_width (GTK_WIDGET (widget));

	gtk_render_focus (context, cr, 0 , 0, width, height);
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

	gtk_widget_set_name (GTK_WIDGET (self), "focus");
	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self), pt_waveviewer_focus_draw, NULL, NULL);

}

static void
pt_waveviewer_focus_class_init (PtWaveviewerFocusClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	widget_class->hierarchy_changed = pt_waveviewer_focus_hierarchy_changed;
}

GtkWidget *
pt_waveviewer_focus_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_FOCUS, NULL));
}
