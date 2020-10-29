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
 * SECTION: pt-waveviewer-cursor
 * @short_description: Internal widget that draws a cursor for PtWaveviewer.
 *
 * PtWaveviewerCursor is part of a GtkOverlay stack, from bottom to top:
 * - PtWaveviewerWaveform
 * - PtWaveviewerSelection
 * - PtWaveviewerCursor
 * - PtWaveviewerFocus
 *
 * pt_waveviewer_cursor_render() is used to render the cursor. The parameter
 * @position is relative to the viewport. That means that the caller has to
 * compute the position from the absolute position (in terms of a fully plotted
 * waveform) minus the left position of the viewport (the value of the horizontal
 * GtkAdjustment).
 *
 * A value of -1 hides the cursor.
 *
 * pt_waveviewer_cursor_set_focus() renders a focus indicator around the cursor.
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
#include "pt-waveviewer.h"
#include "pt-waveviewer-cursor.h"

#define MARKER_BOX_W 10
#define MARKER_BOX_H 8

struct _PtWaveviewerCursorPrivate {
	cairo_surface_t *cursor;
	GdkRGBA	         cursor_color;
	gint             position;
	gboolean         focus;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtWaveviewerCursor, pt_waveviewer_cursor, GTK_TYPE_DRAWING_AREA);


static gboolean
pt_waveviewer_cursor_draw (GtkWidget *widget,
                           cairo_t   *cr)
{
	PtWaveviewerCursor *self = (PtWaveviewerCursor *) widget;

	GtkStyleContext *context;
	gint height;
	gint width;

	height = gtk_widget_get_allocated_height (widget);
	width = gtk_widget_get_allocated_width (widget);

	/* clear everything */
	cairo_set_source_rgba (cr, 0, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	/* paint cursor */
	if (self->priv->position == -1)
		return FALSE;
	cairo_set_source_surface (cr, self->priv->cursor,
	                          self->priv->position - MARKER_BOX_W / 2, 0);
	cairo_paint (cr);

	/* render focus */
	if (self->priv->focus) {
		context = gtk_widget_get_style_context (widget);
		gtk_render_focus (context, cr,
				  self->priv->position - MARKER_BOX_W / 2 - 2,
				  1,
				  MARKER_BOX_W + 4,
				  height - 2);
	}
	return FALSE;
}

static void
draw_cursor (PtWaveviewerCursor *self)
{
	gint height;

	height = gtk_widget_get_allocated_height (GTK_WIDGET (self));
	gtk_widget_queue_draw_area (GTK_WIDGET (self),
				    self->priv->position - MARKER_BOX_W / 2,
				    0,
				    MARKER_BOX_W,
				    height);
}

static void
cache_cursor (PtWaveviewerCursor *self)
{
	if (!gtk_widget_get_realized (GTK_WIDGET (self)))
		return;

	if (self->priv->cursor)
		cairo_surface_destroy (self->priv->cursor);

	cairo_t *cr;
	gint height = gtk_widget_get_allocated_height (GTK_WIDGET (self));

	self->priv->cursor = gdk_window_create_similar_surface (gtk_widget_get_window (GTK_WIDGET (self)),
	                                             CAIRO_CONTENT_COLOR_ALPHA,
	                                             MARKER_BOX_W,
	                                             height);

	cr = cairo_create (self->priv->cursor);

	gdk_cairo_set_source_rgba (cr, &self->priv->cursor_color);

	cairo_move_to (cr, 0 + MARKER_BOX_W / 2, height);
	cairo_line_to (cr, 0 + MARKER_BOX_W / 2, 0);
	cairo_stroke (cr);
	cairo_move_to (cr, 0, 0);
	cairo_line_to (cr, 0 + MARKER_BOX_W, 0);
	cairo_line_to (cr, 0 + MARKER_BOX_W / 2 , 0 + MARKER_BOX_H);
	cairo_line_to (cr, 0, 0);
	cairo_fill (cr);

	cairo_destroy (cr);
}

static void
pt_waveviewer_cursor_size_allocate (GtkWidget     *widget,
                                    GtkAllocation *rectangle)
{
	GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->size_allocate (widget, rectangle);
	/* If widget changed vertical size, cursor’s size has to be adjusted */
	cache_cursor (PT_WAVEVIEWER_CURSOR (widget));
}

static void
update_cached_style_values (PtWaveviewerCursor *self)
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
	gtk_style_context_get_color (context, state, &self->priv->cursor_color);
	cache_cursor (self);
}

static void
pt_waveviewer_cursor_state_flags_changed (GtkWidget     *widget,
                                          GtkStateFlags  flags)
{
	update_cached_style_values (PT_WAVEVIEWER_CURSOR (widget));
	GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->state_flags_changed (widget, flags);
}

static void
pt_waveviewer_cursor_style_updated (GtkWidget *widget)
{
	PtWaveviewerCursor *self = (PtWaveviewerCursor *) widget;

	GTK_WIDGET_CLASS (pt_waveviewer_cursor_parent_class)->style_updated (widget);

	update_cached_style_values (self);
	draw_cursor (self);
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
	if (self->priv->position == position)
		return;
	/* first erase old position */
	draw_cursor (self);
	self->priv->position = position;
	draw_cursor (self);
}

void
pt_waveviewer_cursor_set_focus (PtWaveviewerCursor *self,
                                gboolean            focus)
{
	if (self->priv->focus == focus)
		return;
	self->priv->focus = focus;
	draw_cursor (self);
}

static void
pt_waveviewer_cursor_init (PtWaveviewerCursor *self)
{
	self->priv = pt_waveviewer_cursor_get_instance_private (self);

	GtkStyleContext *context;

	self->priv->cursor = NULL;
	self->priv->focus = FALSE;
	self->priv->position = -1;

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, "cursor");

	gtk_widget_set_events (GTK_WIDGET (self), GDK_ALL_EVENTS_MASK);
}

static void
pt_waveviewer_cursor_finalize (GObject *object)
{
	PtWaveviewerCursor *self = PT_WAVEVIEWER_CURSOR (object);

	if (self->priv->cursor)
		cairo_surface_destroy (self->priv->cursor);

	G_OBJECT_CLASS (pt_waveviewer_cursor_parent_class)->finalize (object);
}

static void
pt_waveviewer_cursor_class_init (PtWaveviewerCursorClass *klass)
{
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	gobject_class->finalize           = pt_waveviewer_cursor_finalize;
	widget_class->draw                = pt_waveviewer_cursor_draw;
	widget_class->realize             = pt_waveviewer_cursor_realize;
	widget_class->size_allocate       = pt_waveviewer_cursor_size_allocate;
	widget_class->state_flags_changed = pt_waveviewer_cursor_state_flags_changed;
	widget_class->style_updated       = pt_waveviewer_cursor_style_updated;
}

GtkWidget *
pt_waveviewer_cursor_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_CURSOR, NULL));
}
