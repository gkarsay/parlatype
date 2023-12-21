/* Copyright (C) Gabor Karsay 2016, 2018, 2019 <gabor.karsay@gmx.at>
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

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <parlatype.h>
#include "pt-app.h"
#include "pt-asr-dialog.h"
#include "pt-config-list.h"
#include "pt-config-row.h"
#include "pt-prefs-info-row.h"
#include "pt-preferences.h"

#define EXAMPLE_TIME_SHORT 471123
#define EXAMPLE_TIME_LONG 4071123

static GtkWidget *preferences_dialog = NULL;

struct _PtPreferencesDialog
{
  AdwPreferencesWindow parent;

  GSettings *editor;
  PtPlayer *player;
  PtConfigList *list;

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
  GtkWidget *preview_group;

  /* ASR page */
  GtkWidget *asr_page;
  GtkWidget *models_group;
  GPtrArray *config_rows;
  gchar *active_config_path;
};

G_DEFINE_FINAL_TYPE (PtPreferencesDialog, pt_preferences_dialog, ADW_TYPE_PREFERENCES_WINDOW)

static void
update_timestamp_page (PtPreferencesDialog *self)
{
  gchar *preview1;
  gchar *preview2;
  gchar *preview;
  guint precision;

  preview1 = pt_player_get_timestamp_for_time (self->player, EXAMPLE_TIME_SHORT, EXAMPLE_TIME_SHORT);
  preview2 = pt_player_get_timestamp_for_time (self->player, EXAMPLE_TIME_LONG, EXAMPLE_TIME_LONG);
  preview = g_strdup_printf ("%s\n%s", preview1, preview2);
  adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (self->preview_group), preview);

  precision = adw_combo_row_get_selected (ADW_COMBO_ROW (self->precision_row));
  gtk_widget_set_sensitive (self->separator_row,
                            (precision == 0) ? FALSE : TRUE);

  g_free (preview1);
  g_free (preview2);
  g_free (preview);
}

static void
settings_changed_cb (GSettings *settings,
                     gchar *key,
                     gpointer user_data)
{
  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (user_data);
  if (g_str_has_prefix (key, "timestamp"))
    {
      update_timestamp_page (self);
    }
  if (g_str_equal (key, "asr-config"))
    {
      gchar *new_value = g_settings_get_string (settings, key);
      g_free (self->active_config_path);
      self->active_config_path = new_value;
    }
}

/* Setting "fixed-cursor" TRUE  -> Dropdown pos. 0 ("Fixed cursor")
 * Setting "fixed-cursor" FALSE -> Dropdown pos. 1 ("Moving cursor") */
static gboolean
get_cursor_mapping (GValue *value,
                    GVariant *variant,
                    gpointer data)
{
  guint drop_down_position = g_variant_get_boolean (variant) ? 0 : 1;
  g_value_set_uint (value, drop_down_position);
  return TRUE;
}

static GVariant *
set_cursor_mapping (const GValue *value,
                    const GVariantType *type,
                    gpointer data)
{
  gboolean fixed = (g_value_get_uint (value) == 0);
  return g_variant_new_boolean (fixed);
}

static gboolean
get_separator_mapping (GValue *value,
                       GVariant *variant,
                       gpointer data)
{
  const gchar *sep = g_variant_get_string (variant, NULL);
  guint drop_down_position = (g_strcmp0 (sep, ".") == 0) ? 0 : 1;
  g_value_set_uint (value, drop_down_position);
  return TRUE;
}

static GVariant *
set_separator_mapping (const GValue *value,
                       const GVariantType *type,
                       gpointer data)
{
  guint drop_down_position = g_value_get_uint (value);
  gchar *sep = (drop_down_position == 0) ? "." : "-";
  return g_variant_new_string (sep);
}

static gboolean
get_delimiter_mapping (GValue *value,
                       GVariant *variant,
                       gpointer data)
{
  const gchar *sep = g_variant_get_string (variant, NULL);
  gint drop_down_position;
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
set_delimiter_mapping (const GValue *value,
                       const GVariantType *type,
                       gpointer data)
{
  guint drop_down_position = g_value_get_uint (value);
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
asr_dialog_destroy_cb (PtAsrDialog *dialog,
                       gpointer user_data)
{
  /* Keep it simple, refresh widgets unconditionally.
   * Possible changes are name change, installed/uninstalled,
   * active/not active, deleted. These changes affect also sorting. */

  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (user_data);
  pt_config_list_refresh (self->list);
}

static void
asr_row_activated (AdwActionRow *row,
                   gpointer user_data)
{
  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (user_data);
  PtConfig *config;

  config = pt_config_row_get_config (PT_CONFIG_ROW (row));
  PtAsrDialog *asr_dlg = pt_asr_dialog_new (GTK_WINDOW (self));
  pt_asr_dialog_set_config (asr_dlg, config);
  g_signal_connect (asr_dlg, "destroy", G_CALLBACK (asr_dialog_destroy_cb), self);
  gtk_window_present (GTK_WINDOW (asr_dlg));
}

static void
items_changed_cb (GListModel *list,
                  guint position, /* bogus */
                  guint added,    /* bogus */
                  guint removed,  /* bogus */
                  gpointer user_data)
{
  /* PtConfigList implements GListModel in a very simplified way.
   * This signal means that everything might have changed. */

  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (user_data);
  PtConfig *config;
  guint n_configs = g_list_model_get_n_items (G_LIST_MODEL (self->list));
  guint n_rows = self->config_rows->len;
  GFile *config_file;
  gchar *config_path;
  gboolean active;

  /* If there are more widgets than needed, unparent them and remove them from pointer array */
  if (n_rows > n_configs)
    {
      for (uint i = n_configs; i < n_rows; i++)
        {
          PtConfigRow *row = g_ptr_array_index (self->config_rows, i);
          adw_preferences_group_remove (ADW_PREFERENCES_GROUP (self->models_group),
                                        GTK_WIDGET (row));
        }
      g_ptr_array_remove_range (self->config_rows, n_configs, n_rows - n_configs);
      g_assert (self->config_rows->len == n_configs);
    }

  for (uint i = 0; i < n_configs; i++)
    {
      config = g_list_model_get_item (G_LIST_MODEL (self->list), i);
      config_file = pt_config_get_file (config);
      config_path = g_file_get_path (config_file);
      active = (g_strcmp0 (self->active_config_path, config_path) == 0) ? TRUE : FALSE;
      g_free (config_path);

      /* If there is already a widget at this index, just update it with new config */
      if (n_rows > 0 && n_rows - 1 >= i)
        {
          PtConfigRow *row = g_ptr_array_index (self->config_rows, i);
          pt_config_row_set_config (row, config);
          pt_config_row_set_active (row, active);
        }

      /* If there is no widget yet, create it, add it to parent and pointer array */
      if (n_rows < i + 1)
        {
          PtConfigRow *asr_row;
          asr_row = pt_config_row_new (config);
          pt_config_row_set_active (asr_row, active);
          g_ptr_array_add (self->config_rows, (gpointer) asr_row);
          g_signal_connect (asr_row, "activated", G_CALLBACK (asr_row_activated), self);
          adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->models_group),
                                     GTK_WIDGET (asr_row));
        }
      g_object_unref (config);
    }
}

static void
import_copy_ready_cb (GObject *source_object,
                      GAsyncResult *res,
                      gpointer user_data)
{
  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (user_data);
  GFile *file = G_FILE (source_object);
  GError *error = NULL;
  gboolean success;
  GtkWindow *parent = GTK_WINDOW (self);
  GtkAlertDialog *err_dialog;

  success = g_file_copy_finish (file, res, &error);

  if (success)
    {
      pt_config_list_refresh (self->list);
    }
  else
    {
      err_dialog = gtk_alert_dialog_new (_ ("Error"));
      gtk_alert_dialog_set_detail (err_dialog, error->message);
      gtk_alert_dialog_show (err_dialog, parent);
      g_error_free (error);
    }
}

static void
dialog_open_cb (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (user_data);
  GFile *file;
  PtConfig *config;
  GtkWindow *parent = GTK_WINDOW (self);
  GtkAlertDialog *err_dialog;
  GFile *dest_folder;
  gchar *dest_folder_path;
  gchar *basename;
  gchar *dest_path;
  GFile *dest;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source),
                                      result,
                                      NULL);

  if (!file)
    return;

  config = pt_config_new (file);

  if (!pt_config_is_valid (config) || !pt_player_config_is_loadable (self->player, config))
    {
      err_dialog = gtk_alert_dialog_new (_ ("Error"));
      gchar *message;
      if (!pt_config_is_valid (config))
        {
          message = _ ("The file is not a valid language model configuration.");
        }
      else
        {
          message = _ ("There is no plugin installed for this language model.");
        }

      gtk_alert_dialog_set_detail (err_dialog, message);

      gtk_alert_dialog_show (err_dialog, parent);
      g_object_unref (config);
      return;
    }

  dest_folder = pt_config_list_get_folder (self->list);
  dest_folder_path = g_file_get_path (dest_folder);
  basename = g_file_get_basename (file);
  dest_path = g_build_path (G_DIR_SEPARATOR_S,
                            dest_folder_path,
                            basename, NULL);
  dest = g_file_new_for_path (dest_path);

  g_file_copy_async (
      file, dest,
      G_FILE_COPY_TARGET_DEFAULT_PERMS,
      G_PRIORITY_DEFAULT,
      NULL,       /* cancellable */
      NULL, NULL, /* progress callback and data*/
      import_copy_ready_cb,
      self);

  g_object_unref (file);
  g_object_unref (dest);
  g_free (basename);
  g_free (dest_folder_path);
  g_free (dest_path);
}

static void
import_button_clicked_cb (GtkButton *button,
                          gpointer user_data)
{
  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (user_data);
  GtkFileDialog *dialog;
  GtkWindow *parent = GTK_WINDOW (self);
  const char *home_path;
  GFile *initial_folder;
  GListStore *filters;
  GtkFileFilter *filter_asr;
  GtkFileFilter *filter_all;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, _ ("Select Model Folder"));

  home_path = g_get_home_dir ();
  initial_folder = g_file_new_for_path (home_path);
  gtk_file_dialog_set_initial_folder (dialog, initial_folder);

  filter_asr = gtk_file_filter_new ();
  filter_all = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter_asr, _ ("Parlatype Language Model Configurations"));
  gtk_file_filter_set_name (filter_all, _ ("All Files"));
  gtk_file_filter_add_pattern (filter_asr, "*.asr");
  gtk_file_filter_add_pattern (filter_all, "*");
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter_asr);
  g_list_store_append (filters, filter_all);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog, parent,
                        NULL, /* cancellable */
                        dialog_open_cb,
                        self);

  g_object_unref (initial_folder);
  /* cleanup? */
}

static void
pt_preferences_dialog_init (PtPreferencesDialog *self)
{
  self->editor = g_settings_new (APP_ID);
  self->player = pt_player_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Waveform page */
  g_settings_bind (
      self->editor, "show-ruler",
      self->ruler_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (
      self->editor, "fixed-cursor",
      self->cursor_row, "selected",
      G_SETTINGS_BIND_DEFAULT,
      get_cursor_mapping,
      set_cursor_mapping,
      NULL, NULL);

  GtkAdjustment *pps_adj;
  pps_adj = gtk_range_get_adjustment (GTK_RANGE (self->pps_scale));
  g_settings_bind (
      self->editor, "pps",
      pps_adj, "value",
      G_SETTINGS_BIND_DEFAULT);

  /* Controls page */
  g_settings_bind (
      self->editor, "rewind-on-pause",
      self->pause_row, "value",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->editor, "jump-back",
      self->back_row, "value",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->editor, "jump-forward",
      self->forward_row, "value",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->editor, "repeat-all",
      self->repeat_all_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->editor, "repeat-selection",
      self->repeat_selection_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  /* Timestamps page */
  /* Bind settings to our player instance for example timestamps */
  g_settings_bind (
      self->editor, "timestamp-precision",
      self->player, "timestamp-precision",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "timestamp-fixed",
      self->player, "timestamp-fixed",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "timestamp-delimiter",
      self->player, "timestamp-delimiter",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "timestamp-fraction-sep",
      self->player, "timestamp-fraction-sep",
      G_SETTINGS_BIND_GET);

  /* setup timestamp fields according to settings */
  g_settings_bind (
      self->editor, "timestamp-fixed",
      self->hours_row, "active",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (
      self->editor, "timestamp-precision",
      self->precision_row, "selected",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (
      self->editor, "timestamp-fraction-sep",
      self->separator_row, "selected",
      G_SETTINGS_BIND_DEFAULT,
      get_separator_mapping,
      set_separator_mapping,
      NULL, NULL);

  g_settings_bind_with_mapping (
      self->editor, "timestamp-delimiter",
      self->delimiter_row, "selected",
      G_SETTINGS_BIND_DEFAULT,
      get_delimiter_mapping,
      set_delimiter_mapping,
      NULL, NULL);

  g_signal_connect (
      self->editor, "changed",
      G_CALLBACK (settings_changed_cb),
      self);

  update_timestamp_page (self);

  self->config_rows = g_ptr_array_new ();
  self->list = pt_config_list_new (self->player);
  g_signal_connect (self->list, "items-changed", G_CALLBACK (items_changed_cb), self);
  self->active_config_path = g_settings_get_string (self->editor, "asr-config");

#ifndef HAVE_ASR
  adw_preferences_window_remove (ADW_PREFERENCES_WINDOW (self),
                                 ADW_PREFERENCES_PAGE (self->asr_page));
#endif
}

static void
pt_preferences_dialog_dispose (GObject *object)
{
  PtPreferencesDialog *self = PT_PREFERENCES_DIALOG (object);

  g_clear_object (&self->editor);
  g_clear_object (&self->player);
  g_ptr_array_free (self->config_rows, TRUE);

  g_free (self->active_config_path);

  G_OBJECT_CLASS (pt_preferences_dialog_parent_class)->dispose (object);
}

static void
pt_preferences_dialog_class_init (PtPreferencesDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_preferences_dialog_dispose;

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/Parlatype/preferences.ui");
  gtk_widget_class_bind_template_callback (widget_class, import_button_clicked_cb);
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
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, asr_page);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, preview_group);
  gtk_widget_class_bind_template_child (widget_class, PtPreferencesDialog, models_group);
}

static void
prefs_destroy_cb (GtkWidget *widget,
                  gpointer user_data)
{
  preferences_dialog = NULL;
}

void
pt_show_preferences_dialog (GtkWindow *parent)
{
  if (preferences_dialog == NULL)
    {

      preferences_dialog = GTK_WIDGET (g_object_new (PT_TYPE_PREFERENCES_DIALOG,
                                                     NULL));
      g_signal_connect (preferences_dialog,
                        "destroy",
                        G_CALLBACK (prefs_destroy_cb),
                        NULL);
    }

  if (parent != gtk_window_get_transient_for (GTK_WINDOW (preferences_dialog)))
    {
      gtk_window_set_transient_for (GTK_WINDOW (preferences_dialog), GTK_WINDOW (parent));
      gtk_window_set_modal (GTK_WINDOW (preferences_dialog), FALSE);
      gtk_window_set_destroy_with_parent (GTK_WINDOW (preferences_dialog), TRUE);
    }

  gtk_window_present (GTK_WINDOW (preferences_dialog));
}

PtPreferencesDialog *
pt_preferences_dialog_new (GtkWindow *parent)
{
  return g_object_new (
      PT_TYPE_PREFERENCES_DIALOG,
      "transient-for", parent,
      NULL);
}
