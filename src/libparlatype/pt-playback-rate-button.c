/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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
 * PtPlaybackRateButton:
 *
 * Controls playback rate.
 */

#include "config.h"

#include "pt-playback-rate-button.h"

#include <math.h> /* round */

struct _PtPlaybackRateButton
{
  AdwBin         parent;
  GtkAdjustment *adjustment;
  GtkWidget     *label;
};

G_DEFINE_FINAL_TYPE (PtPlaybackRateButton, pt_playback_rate_button, ADW_TYPE_BIN)

/**
 * pt_playback_rate_button_get_adjustment:
 * @self: a `PtPlaybackRateButton`
 *
 * Returns an adjustment to read or write playback rate.
 *
 * The value 1.0 represents normal playback rate, the lower bound
 * is 0.5, the higher bound is 3.0.
 *
 * Returns: (transfer none): a `GtkAdjustment`
 */
GtkAdjustment *
pt_playback_rate_button_get_adjustment (PtPlaybackRateButton *self)
{
  return self->adjustment;
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             gpointer       user_data)
{
  PtPlaybackRateButton *self = PT_PLAYBACK_RATE_BUTTON (user_data);

  double rate = gtk_adjustment_get_value (adjustment) * 10;
  rate = round (rate) / 10;
  char *label = g_strdup_printf ("× %.1f", rate);
  gtk_label_set_text (GTK_LABEL (self->label), label);
  g_free (label);
}

static void
pt_playback_rate_button_dispose (GObject *object)
{
  PtPlaybackRateButton *self = PT_PLAYBACK_RATE_BUTTON (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_PLAYBACK_RATE_BUTTON);

  G_OBJECT_CLASS (pt_playback_rate_button_parent_class)->dispose (object);
}

static void
pt_playback_rate_button_init (PtPlaybackRateButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_playback_rate_button_class_init (PtPlaybackRateButtonClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_playback_rate_button_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/playback-rate-button.ui");
  gtk_widget_class_bind_template_callback (widget_class, adjustment_value_changed_cb);
  gtk_widget_class_bind_template_child (widget_class, PtPlaybackRateButton, adjustment);
  gtk_widget_class_bind_template_child (widget_class, PtPlaybackRateButton, label);
}

/**
 * pt_playback_rate_button_new:
 *
 * Constructor for `PtPlaybackRateButton`.
 *
 * Returns: a newly created `PtPlaybackRateButton`.
 */
PtPlaybackRateButton *
pt_playback_rate_button_new (void)
{
  return g_object_new (PT_TYPE_PLAYBACK_RATE_BUTTON, NULL);
}
