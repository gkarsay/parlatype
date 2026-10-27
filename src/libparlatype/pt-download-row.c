/* Copyright 2026 Gabor Karsay <gabor.karsay@gmx.at>
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
 * PtDownloadRow:
 *
 * Starts/cancels a download.
 *
 * Offers a download button to initiate a download. Once a download started, another (internal) row
 * is revealed showing progress and a cancel button.
 *
 * `PtDownloadRow` is a presentational widget without logic.
 *
 * Used in [class@PtConfigView].
 */

#include "config.h"

#include "pt-download-row.h"

#include <glib/gi18n.h>

struct _PtDownloadRow
{
  AdwPreferencesRow parent_instance;

  /* Row with download button */
  AdwActionRow *download_row;
  GtkWidget    *download_button;

  /* Row with progress bar and cancel button */
  GtkWidget      *revealer;
  GtkProgressBar *progress_bar;
  GtkLabel       *progress_label;
  GtkLabel       *rate_label;

  float fraction;
};

enum
{
  PROP_0,
  PROP_REVEALED,
  PROP_FRACTION,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

enum
{
  SIGNAL_START_DOWNLOAD,
  SIGNAL_CANCELLED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (PtDownloadRow, pt_download_row, ADW_TYPE_PREFERENCES_ROW)

static void
cancel_button_clicked_cb (PtDownloadRow *self,
                          GtkButton     *button)
{
  g_signal_emit (self, signals[SIGNAL_CANCELLED], 0, NULL);
}

static void
download_button_clicked_cb (PtDownloadRow *self,
                            GtkButton     *button)
{
  g_signal_emit (self, signals[SIGNAL_START_DOWNLOAD], 0, NULL);
  gtk_widget_set_sensitive (self->download_button, FALSE);
}

/**
 * pt_download_row_get_revealed:
 * @self: a `PtDownloadRow`
 *
 * Gets if progress row is revealed.
 *
 * Returns: Whether the progress row is revealed
 *
 * Since: 5.0
 */
gboolean
pt_download_row_get_revealed (PtDownloadRow *self)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD_ROW (self), FALSE);

  return gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer));
}

/**
 * pt_download_row_set_revealed:
 * @self: a `PtDownloadRow`
 * @revealed: status of progress row
 *
 * Sets whether the progress row should be revealed.
 *
 * Since: 5.0
 */
void
pt_download_row_set_revealed (PtDownloadRow *self,
                              gboolean       revealed)
{
  g_return_if_fail (PT_IS_DOWNLOAD_ROW (self));

  if (gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer)) == revealed)
    return;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), revealed);
  if (!revealed)
    {
      gtk_widget_set_sensitive (self->download_button, TRUE);
    }

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_REVEALED]);
}

/**
 * pt_download_row_set_size_label:
 * @self: a `PtDownloadRow`
 * @label: a size string
 *
 * Sets the label for the total size of the download.
 *
 * Since: 5.0
 */
void
pt_download_row_set_size_label (PtDownloadRow *self,
                                char          *label)
{
  g_return_if_fail (PT_IS_DOWNLOAD_ROW (self));
  adw_action_row_set_subtitle (self->download_row, label);
}

/**
 * pt_download_row_set_remaining_label:
 * @self: a `PtDownloadRow`
 * @label: a time string
 *
 * Set the label for the remaining time of the download.
 *
 * Since: 5.0
 */
void
pt_download_row_set_remaining_label (PtDownloadRow *self,
                                     char          *label)
{
  g_return_if_fail (PT_IS_DOWNLOAD_ROW (self));
  gtk_label_set_text (self->rate_label, label);
}

/**
 * pt_download_row_set_progress_label:
 * @self: self
 * @label: a size string (current / total)
 *
 * Set the label for the current progress of the download.
 *
 * Since: 5.0
 */
void
pt_download_row_set_progress_label (PtDownloadRow *self,
                                    char          *label)
{
  g_return_if_fail (PT_IS_DOWNLOAD_ROW (self));
  gtk_label_set_text (self->progress_label, label);
}

/**
 * pt_download_row_set_progress_bar:
 * @self: a `PtDownloadRow`
 * @progress: value between 0.0 and 1.0
 *
 * Sets the progress bar.
 *
 * Since: 5.0
 */
void
pt_download_row_set_progress_bar (PtDownloadRow *self,
                                  float          progress)
{
  g_return_if_fail (PT_IS_DOWNLOAD_ROW (self));
  gtk_progress_bar_set_fraction (self->progress_bar, progress);
}

static void
pt_download_row_dispose (GObject *object)
{
  PtDownloadRow *self = PT_DOWNLOAD_ROW (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_DOWNLOAD_ROW);

  G_OBJECT_CLASS (pt_download_row_parent_class)->dispose (object);
}

static void
pt_download_row_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PtDownloadRow *self = PT_DOWNLOAD_ROW (object);

  switch (property_id)
    {
    case PROP_REVEALED:
      pt_download_row_set_revealed (self, g_value_get_boolean (value));
      break;
    case PROP_FRACTION:
      self->fraction = g_value_get_float (value);
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), self->fraction);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_row_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  PtDownloadRow *self = PT_DOWNLOAD_ROW (object);

  switch (property_id)
    {
    case PROP_REVEALED:
      g_value_set_boolean (value, pt_download_row_get_revealed (self));
      break;
    case PROP_FRACTION:
      g_value_set_float (value, self->fraction);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_row_init (PtDownloadRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_download_row_class_init (PtDownloadRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = pt_download_row_set_property;
  object_class->get_property = pt_download_row_get_property;
  object_class->dispose = pt_download_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/download-row.ui");
  gtk_widget_class_bind_template_child (widget_class, PtDownloadRow, revealer);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadRow, download_row);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadRow, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadRow, download_button);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadRow, progress_label);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadRow, rate_label);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, download_button_clicked_cb);

  /**
   * PtDownloadRow::start-download:
   * @self: the instance emitting the signal
   *
   * Emitted on download button clicked.
   */
  signals[SIGNAL_START_DOWNLOAD] = g_signal_new ("start-download",
                                                 PT_TYPE_DOWNLOAD_ROW,
                                                 G_SIGNAL_RUN_LAST,
                                                 0, NULL, NULL, NULL,
                                                 G_TYPE_NONE, 0);

  /**
   * PtDownloadRow::cancelled:
   * @self: the instance emitting the signal
   *
   * Emitted on cancel button clicked.
   */
  signals[SIGNAL_CANCELLED] = g_signal_new ("cancelled",
                                            PT_TYPE_DOWNLOAD_ROW,
                                            G_SIGNAL_RUN_LAST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE, 0);

  /**
   * PtDownloadRow:revealed:
   *
   * Describe me.
   */
  obj_properties[PROP_REVEALED] =
      g_param_spec_boolean (
          "revealed", NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * PtDownloadRow:fraction:
   *
   * Progress as shown on progress bar, between 0.0 and 1.0.
   */
  obj_properties[PROP_FRACTION] =
      g_param_spec_float (
          "fraction", NULL, NULL,
          0, 1, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_download_row_new:
 *
 * Returns a new PtDownloadRow instance for the config.
 *
 * Returns: (transfer full): a new #PtDownloadRow
 */
PtDownloadRow *
pt_download_row_new (void)
{
  return g_object_new (PT_TYPE_DOWNLOAD_ROW, NULL);
}
