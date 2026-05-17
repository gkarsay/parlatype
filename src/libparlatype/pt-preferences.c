/* Copyright 2016 Gabor Karsay <gabor.karsay@gmx.at>
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
 * PtPreferencesDialog:
 *
 * Preferences dialog.
 */

#include "config.h"

#include "pt-preferences.h"

#include "pt-app.h"
#include "pt-window-private.h"
#include "pt-window.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <parlatype.h>

struct _PtPreferencesDialog
{
  AdwPreferencesDialog parent;

  GSettings *settings;
  GtkWindow *parent_window;

  /* Waveform page */
  GtkWidget *pps_scale;
  GtkWidget *ruler_row;
  GtkWidget *cursor_row;

  /* Controls page */
  GtkWidget *pause_row;
  GtkWidget *back_row;
  GtkWidget *forward_row;
  GtkWidget *repeat_all_row;
  GtkWidget *repeat_selection_row;

  /* Timestamps page */
  GtkWidget *hours_row;
  GtkWidget *precision_row;
  GtkWidget *separator_row;
  GtkWidget *delimiter_row;

  ulong handler_id;
};

G_DEFINE_FINAL_TYPE (PtPreferencesDialog, pt_preferences_dialog, ADW_TYPE_PREFERENCES_DIALOG)

/* Setting "fixed-cursor" TRUE  -> Dropdown pos. 0 ("Fixed cursor")
 * Setting "fixed-cursor" FALSE -> Dropdown pos. 1 ("Moving cursor") */
static gboolean
get_cursor_mapping (GValue   *value,
                    GVariant *variant,
                    gpointer  data)
{
  guint drop_down_position = g_variant_get_boolean (variant) ? 0 : 1;
  g_value_set_uint (value, drop_down_position);
  return TRUE;
}

static GVariant *
set_cursor_mapping (const GValue       *value,
                    const GVariantType *type,
                    gpointer            data)
{
  gboolean fixed = (g_value_get_uint (value) == 0);
  return g_variant_new_boolean (fixed);
}

static gboolean
get_separator_mapping (GValue   *value,
                       GVariant *variant,
                       gpointer  data)
{
  const gchar *sep = g_variant_get_string (variant, NULL);
  guint        drop_down_position = (g_strcmp0 (sep, ".") == 0) ? 0 : 1;
  g_value_set_uint (value, drop_down_position);
  return TRUE;
}

static GVariant *
set_separator_mapping (const GValue       *value,
                       const GVariantType *type,
                       gpointer            data)
{
  guint  drop_down_position = g_value_get_uint (value);
  gchar *sep = (drop_down_position == 0) ? "." : "-";
  return g_variant_new_string (sep);
}

static gboolean
get_delimiter_mapping (GValue   *value,
                       GVariant *variant,
                       gpointer  data)
{
  const gchar *sep = g_variant_get_string (variant, NULL);
  int          drop_down_position;
  if (g_strcmp0 (sep, "None") == 0)
    drop_down_position = 0;
  else if (g_strcmp0 (sep, "#") == 0)
    drop_down_position = 1;
  else if (g_strcmp0 (sep, "(") == 0)
    drop_down_position = 2;
  else if (g_strcmp0 (sep, "[") == 0)
    drop_down_position = 3;
  else
    return FALSE;

  g_value_set_uint (value, drop_down_position);
  return TRUE;
}

static GVariant *
set_delimiter_mapping (const GValue       *value,
                       const GVariantType *type,
                       gpointer            data)
{
  guint  drop_down_position = g_value_get_uint (value);
  gchar *sep = NULL;

  if (drop_down_position == 0)
    sep = "None";
  else if (drop_down_position == 1)
    sep = "#";
  else if (drop_down_position == 2)
    sep = "(";
  else if (drop_down_position == 3)
    sep = "[";

  return g_variant_new_string (sep);
}

static void
pt_preferences_dialog_init (PtPreferencesDialog *self)
{
  self->settings = g_settings_new (APP_ID);
  self->parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Waveform page */
  g_settings_bind (
      self->settings, "show-ruler",
      self->ruler_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (
      self->settings, "fixed-cursor",
      self->cursor_row, "selected",
      G_SETTINGS_BIND_DEFAULT,
      get_cursor_mapping,
      set_cursor_mapping,
      NULL, NULL);

  GtkAdjustment *pps_adj;
  pps_adj = gtk_range_get_adjustment (GTK_RANGE (self->pps_scale));
  g_settings_bind (
      self->settings, "pps",
      pps_adj, "value",
      G_SETTINGS_BIND_DEFAULT);

  /* Controls page */
  g_settings_bind (
      self->settings, "rewind-on-pause",
      self->pause_row, "value",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->settings, "jump-back",
      self->back_row, "value",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->settings, "jump-forward",
      self->forward_row, "value",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->settings, "repeat-all",
      self->repeat_all_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->settings, "repeat-selection",
      self->repeat_selection_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  /* Timestamps page */
  g_settings_bind (
      self->settings, "timestamp-fixed",
      self->hours_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->settings, "timestamp-precision",
      self->precision_row, "selected",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (
      self->settings, "timestamp-fraction-sep",
      self->separator_row, "selected",
      G_SETTINGS_BIND_DEFAULT,
      get_separator_mapping,
      set_separator_mapping,
      NULL, NULL);

  g_settings_bind_with_mapping (
      self->settings, "timestamp-delimiter",
      self->delimiter_row, "selected",
      G_SETTINGS_BIND_DEFAULT,
      get_delimiter_mapping,
      set_delimiter_mapping,
      NULL, NULL);
}

static void
pt_preferences_dialog_dispose (GObject *object)
{
  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (object);

  /* Disconnect early because of race condition in screenshot tool */
  g_clear_signal_handler (&self->handler_id, self->settings);

  gtk_widget_unparent (GTK_WIDGET (self));
  g_clear_object (&self->settings);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_PREFERENCES_DIALOG);

  G_OBJECT_CLASS (pt_preferences_dialog_parent_class)->dispose (object);
}

static void
pt_preferences_dialog_class_init (PtPreferencesDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_preferences_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, pps_scale);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, ruler_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, cursor_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, pause_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, back_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, forward_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, repeat_all_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, repeat_selection_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, hours_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, precision_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, separator_row);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, delimiter_row);
}

/**
 * pt_preferences_dialog_new:
 *
 * Constructor für preferences dialog.
 *
 * Returns: a newly created `PtPreferencesDialog`.
 */
PtPreferencesDialog *
pt_preferences_dialog_new (void)
{
  return g_object_new (PT_TYPE_PREFERENCES_DIALOG, NULL);
}
