/* Copyright (C) 2020 Gabor Karsay <gabor.karsay@gmx.at>
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
 * pt-waveviewer-scrollbox
 * Internal viewport widget for PtWaveviewer.
 *
 * This is a viewport that pretends to have children with the width of a fully
 * plotted waveform. A regular viewport would scroll its children to the
 * desired position, however, a fully plotted waveform would be too wide and
 * would not work with GTK 4 (it is possible in GTK 3 by clipping the drawing
 * area to the visible extents).

 * PtWaveviewerScrollbox doesn't do anything except creating a horizontal
 * GtkAdjustment. Notably it doesn't scroll its children.
 *
 * Use pt_waveviewer_scrollbox_set() and its parameter @width to set the fake
 * width of the children, that is the width of the fully plotted waveform.
 *
 * Most children (waveform, ruler, selection) get the parent's GtkAdjustment and
 * decide on their own what to show when there is a scroll event.
 *
 * The focus widget doesn't care about scroll events and shows a focus
 * indicator close to the viewport border.
 *
 * Only the cursor widget has to be constantly updated by PtWaveviewer.
 */


#include "config.h"
#include "pt-waveviewer.h"
#include "pt-waveviewer-scrollbox.h"


struct _PtWaveviewerScrollboxPrivate {
	GtkAdjustment *adjustment;
	guint hscroll_policy : 1;
	gint fake_width;
};

enum {
	PROP_0,
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY
};

G_DEFINE_TYPE_WITH_CODE (PtWaveviewerScrollbox, pt_waveviewer_scrollbox, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (PtWaveviewerScrollbox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL));


static void
pt_waveviewer_scrollbox_set_adjustment_values (PtWaveviewerScrollbox *self)
{
	/* Set values depending on waveform array size */

	GtkAllocation  allocation;
	GtkAdjustment *adj = self->priv->adjustment;

	gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

	//g_print ("configure adjustment\n");
	gtk_adjustment_configure (adj,
			gtk_adjustment_get_value (adj),
			0.0,
			MAX (self->priv->fake_width, allocation.width),
			allocation.width * 0.2,
			allocation.width * 0.9,
			allocation.width);
}

static void
scrollbox_set_adjustment (PtWaveviewerScrollbox *self,
                          GtkAdjustment         *adjustment)
{
	if (adjustment && self->priv->adjustment == adjustment)
		/* same pointer or both NULL */
		return;

	if (self->priv->adjustment != NULL) {
		g_signal_handlers_disconnect_matched (
				self->priv->adjustment,
				G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, self);
		g_object_unref (self->priv->adjustment);
	}

	if (!adjustment)
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	self->priv->adjustment = g_object_ref_sink (adjustment);

	pt_waveviewer_scrollbox_set_adjustment_values (self);

	g_object_notify (G_OBJECT (self), "hadjustment");
}

void
pt_waveviewer_scrollbox_set (PtWaveviewerScrollbox *self,
                             gint                   width)
{
	self->priv->fake_width = width;
	pt_waveviewer_scrollbox_set_adjustment_values (self);
}

static void
pt_waveviewer_scrollbox_size_allocate (GtkWidget     *widget,
                                       GtkAllocation *allocation)
{
	GTK_WIDGET_CLASS (pt_waveviewer_scrollbox_parent_class)->size_allocate (widget, allocation);

	pt_waveviewer_scrollbox_set_adjustment_values (PT_WAVEVIEWER_SCROLLBOX (widget));
}

static void
pt_waveviewer_scrollbox_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	PtWaveviewerScrollbox *self = PT_WAVEVIEWER_SCROLLBOX (object);
	PtWaveviewerScrollboxPrivate *priv = self->priv;

	switch (property_id) {
	case PROP_HADJUSTMENT:
		g_value_set_object (value, priv->adjustment);
		break;
	case PROP_VADJUSTMENT:
		g_value_set_object (value, NULL);
		break;
	case PROP_HSCROLL_POLICY:
		g_value_set_enum (value, priv->hscroll_policy);
		break;
	case PROP_VSCROLL_POLICY:
		g_value_set_enum (value, 1);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveviewer_scrollbox_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	PtWaveviewerScrollbox *self = PT_WAVEVIEWER_SCROLLBOX (object);
	PtWaveviewerScrollboxPrivate *priv = self->priv;

	switch (property_id) {
	case PROP_HADJUSTMENT:
		scrollbox_set_adjustment (self, g_value_get_object (value));
		break;
	case PROP_VADJUSTMENT:
		/* there is no vadjustment */
		break;
	case PROP_HSCROLL_POLICY:
		if (priv->hscroll_policy != g_value_get_enum (value)) {
			priv->hscroll_policy = g_value_get_enum (value);
			gtk_widget_queue_resize (GTK_WIDGET (self));
			g_object_notify_by_pspec (object, pspec);
		}
		break;
	case PROP_VSCROLL_POLICY:
		/* there is no vadjustment */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveviewer_scrollbox_dispose (GObject *object)
{
	PtWaveviewerScrollbox *self = PT_WAVEVIEWER_SCROLLBOX (object);
	PtWaveviewerScrollboxPrivate *priv = self->priv;

	g_clear_object (&priv->adjustment);

	G_OBJECT_CLASS (pt_waveviewer_scrollbox_parent_class)->dispose (object);
}

static void
pt_waveviewer_scrollbox_init (PtWaveviewerScrollbox *self)
{
	self->priv = pt_waveviewer_scrollbox_get_instance_private (self);

	self->priv->adjustment = NULL;
	self->priv->fake_width = 0;

	gtk_widget_set_name (GTK_WIDGET (self), "scrollbox");
}

static void
pt_waveviewer_scrollbox_class_init (PtWaveviewerScrollboxClass *klass)
{
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = pt_waveviewer_scrollbox_set_property;
	gobject_class->get_property = pt_waveviewer_scrollbox_get_property;
	gobject_class->dispose      = pt_waveviewer_scrollbox_dispose;

	widget_class->size_allocate = pt_waveviewer_scrollbox_size_allocate;

	/* Implementation of GTK_SCROLLABLE */
	g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
	g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
	g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");
}

GtkWidget *
pt_waveviewer_scrollbox_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVEVIEWER_SCROLLBOX,
					 "orientation", GTK_ORIENTATION_VERTICAL,
					 NULL));
}
