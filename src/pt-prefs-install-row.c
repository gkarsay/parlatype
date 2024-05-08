/* Copyright 2023 Gabor Karsay <gabor.karsay@gmx.at>
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

#include "pt-prefs-install-row.h"

#include "pt-asr-dialog.h"

#include <glib/gi18n.h>

struct _PtPrefsInstallRow
{
  AdwActionRow parent;

  PtConfig  *config;
  gboolean   installed;
  gchar     *current_path;
  GtkWindow *parent_window;
};

enum
{
  PROP_CONFIG = 1,
  PROP_INSTALLED,
  N_PROPERTIES
};

static GParamSpec *props[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtPrefsInstallRow, pt_prefs_install_row, ADW_TYPE_ACTION_ROW)

static void
update_row (PtPrefsInstallRow *self,
            gchar             *folder)
{
  gchar *title, *subtitle;

  if (self->installed)
    {
      title = _ ("Installed");
      subtitle = folder;
    }
  else
    {
      title = _ ("Not Installed");
      subtitle = _ ("Download files and select model folder.");
    }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), subtitle);
}

static void
dialog_open_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  PtPrefsInstallRow *self = PT_PREFS_INSTALL_ROW (user_data);
  GFile             *folder;
  gchar             *path = NULL;
  gboolean           installed;

  folder = gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (source),
                                                 result,
                                                 NULL);

  if (folder)
    {
      path = g_file_get_path (folder);
      g_object_unref (folder);
    }

  if (!path)
    return;

  pt_config_set_base_folder (self->config, path);
  installed = pt_config_is_installed (self->config);
  if (installed)
    {
      if (g_strcmp0 (path, self->current_path) != 0)
        {
          g_free (self->current_path);
          self->current_path = g_strdup (path);
        }
      if (!self->installed)
        {
          self->installed = TRUE;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLED]);
        }
      update_row (self, path);
    }

  if (!installed)
    {
      /* If model data was installed before, reset to known good path again. */
      if (self->installed)
        pt_config_set_base_folder (self->config, self->current_path);

      GtkAlertDialog *alert;
      alert = gtk_alert_dialog_new (_ ("Model Data Not Found"));
      gtk_alert_dialog_set_detail (alert,
                                   _ ("The chosen folder does not contain data for this configuration."));
      gtk_alert_dialog_show (alert, self->parent_window);
      g_object_unref (alert);
    }
  g_free (path);
}

static void
folder_button_clicked_cb (GtkButton *widget,
                          gpointer   user_data)
{
  PtPrefsInstallRow *self = PT_PREFS_INSTALL_ROW (user_data);
  GtkFileDialog     *dialog;
  GFile             *initial_folder;

  if (!self->parent_window)
    self->parent_window = GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (self),
                                                               PT_TYPE_ASR_DIALOG));
  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, _ ("Select Model Folder"));

  initial_folder =
      g_file_new_for_path (self->installed ? self->current_path
                                           : g_get_home_dir ());

  gtk_file_dialog_set_initial_folder (dialog, initial_folder);

  gtk_file_dialog_select_folder (dialog, self->parent_window,
                                 NULL, /* cancellable */
                                 dialog_open_cb,
                                 self);

  g_object_unref (initial_folder);
}

gboolean
pt_prefs_install_row_get_installed (PtPrefsInstallRow *self)
{
  return self->installed;
}

static void
pt_prefs_install_row_finalize (GObject *object)
{
  PtPrefsInstallRow *self = PT_PREFS_INSTALL_ROW (object);

  g_free (self->current_path);

  G_OBJECT_CLASS (pt_prefs_install_row_parent_class)->finalize (object);
}

static void
pt_prefs_install_row_constructed (GObject *object)
{
  PtPrefsInstallRow *self = PT_PREFS_INSTALL_ROW (object);

  self->current_path = g_strdup (pt_config_get_base_folder (self->config));
  self->installed = self->current_path && self->current_path[0] && pt_config_is_installed (self->config);

  update_row (self, self->current_path);
}

static void
pt_prefs_install_row_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  PtPrefsInstallRow *self = PT_PREFS_INSTALL_ROW (object);

  switch (property_id)
    {
    case PROP_INSTALLED:
      g_value_set_boolean (value, self->installed);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_prefs_install_row_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  PtPrefsInstallRow *self = PT_PREFS_INSTALL_ROW (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      self->config = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_prefs_install_row_init (PtPrefsInstallRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_prefs_install_row_class_init (PtPrefsInstallRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = pt_prefs_install_row_constructed;
  object_class->finalize = pt_prefs_install_row_finalize;
  object_class->get_property = pt_prefs_install_row_get_property;
  object_class->set_property = pt_prefs_install_row_set_property;

  props[PROP_CONFIG] =
      g_param_spec_object ("config", NULL, NULL,
                           PT_TYPE_CONFIG,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_INSTALLED] =
      g_param_spec_boolean ("installed", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/prefs-install-row.ui");
  gtk_widget_class_bind_template_callback (widget_class, folder_button_clicked_cb);
}

GtkWidget *
pt_prefs_install_row_new (PtConfig *config)
{
  return GTK_WIDGET (g_object_new (PT_TYPE_PREFS_INSTALL_ROW,
                                   "config", config,
                                   NULL));
}
