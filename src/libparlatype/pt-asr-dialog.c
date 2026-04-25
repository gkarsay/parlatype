/* Copyright 2021 Gabor Karsay <gabor.karsay@gmx.at>
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
 * PtAsrDialog:
 *
 * A dialog for a single ASR configuration.
 *
 * It presents most fields of a `PtConfig`, some of them can be changed, and
 * allows for activating, deactivating or deleting this config.
 */

#include "config.h"

#include "pt-asr-dialog.h"

#include "pt-downloader.h"
#include "pt-preferences.h"
#include "pt-prefs-info-row.h"
#include "pt-prefs-install-row.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libpeas.h>
#include <parlatype.h>

struct _PtAsrDialog
{
  AdwPreferencesDialog parent;

  PtConfig     *config;
  GSettings    *editor;
  PtAsrManager *asr_manager;

  GtkWidget    *page;
  GtkWidget    *status_row;
  GtkWidget    *name_row;
  GtkWidget    *info_group;
  GtkWidget    *install_group;
  GtkWidget    *delete_group;
  GtkWidget    *delete_row;
  GtkWidget    *activate_button;
  GtkWidget    *download_row;
  GtkWidget    *download_button;
  GtkWidget    *cancel_button;
  GtkWidget    *progress_bar;
  GCancellable *cancel;

  gboolean active;
  gboolean installed;
};

G_DEFINE_FINAL_TYPE (PtAsrDialog, pt_asr_dialog, ADW_TYPE_PREFERENCES_DIALOG)

static void
name_row_apply_cb (GtkEditable *editable,
                   gpointer     user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  gchar       *old_name, *new_name;

  old_name = pt_config_get_name (self->config);
  new_name = g_strdup (gtk_editable_get_text (editable));
  new_name = g_strchug (new_name);
  new_name = g_strchomp (new_name);

  if (g_strcmp0 (new_name, "") == 0)
    {
      gtk_editable_set_text (editable, old_name);
      g_free (new_name);
      return;
    }

  if (g_strcmp0 (new_name, old_name) != 0)
    {
      adw_dialog_set_title (ADW_DIALOG (self), new_name);
      pt_config_set_name (self->config, new_name);
    }

  g_free (new_name);
}

static gboolean
have_string (gchar *str)
{
  return (str && g_strcmp0 (str, "") != 0);
}

/* GtkLinkButton with label set to host */
static GtkWidget *
pt_download_button_new (gchar *link)
{
  GtkWidget   *link_button;
  GUri        *uri;
  const gchar *host = NULL;

  uri = g_uri_parse (link, G_URI_FLAGS_NONE, NULL);
  if (uri)
    {
      host = g_uri_get_host (uri);
    }

  /* Translators: Fallback label for a download location */
  link_button = gtk_link_button_new_with_label (link, host ? host : _ ("Link"));
  g_uri_unref (uri);
  return link_button;
}

static void
update_download_row (PtAsrDialog *self)
{
  if (self->installed)
    {
      gtk_widget_set_sensitive (self->download_button, FALSE);
    }
  else
    {
      if (pt_config_get_downloader (self->config) != NULL)
        {
          gtk_widget_set_sensitive (self->download_button, TRUE);
        }
    }
}

static void
update_status_row (PtAsrDialog *self)
{
  gchar     *title;
  gchar     *subtitle;
  gchar     *button_label;
  GtkWidget *active_image;

  if (self->active)
    {
      /* Not using title case here, this is more a status than a title. */
      title = _ ("This configuration is active.");
      subtitle = "";
      button_label = _ ("Deactivate");
      active_image = gtk_image_new_from_icon_name ("object-select-symbolic");
      adw_action_row_add_prefix (ADW_ACTION_ROW (self->status_row), active_image);
    }
  else if (self->installed)
    {
      /* Translators: Model is a language model */
      title = _ ("Model data is installed.");
      subtitle = "";
      button_label = _ ("Activate");
    }
  else
    {
      /* Translators: Model is a language model */
      title = _ ("Model data is not installed.");
      subtitle = _ ("See next section for details.");
      button_label = _ ("Activate");
    }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->status_row), title);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->status_row), subtitle);
  gtk_button_set_label (GTK_BUTTON (self->activate_button), button_label);
  gtk_widget_set_sensitive (self->activate_button, self->installed);

  if (self->installed)
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->delete_row),
                                 /* Translators: Configuration will be deleted, but not its data */
                                 _ ("Model data will be preserved"));
  else
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->delete_row), NULL);

  update_download_row (self);
}

static void
installed_cb (PtPrefsInstallRow *row,
              GParamSpec        *pspec,
              gpointer           user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  gboolean     success;

  self->installed = pt_prefs_install_row_get_installed (row);

  if (!self->installed && self->active)
    {
      success = g_settings_set_string (self->editor, "asr-config", "");
      if (success)
        {
          self->active = FALSE;
        }
    }

  update_status_row (self);
}

static void
add_info_row (PtAsrDialog *self,
              gchar       *title,
              gchar       *subtitle)
{
  GtkWidget *row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);
  gtk_widget_add_css_class (row, "property");
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->info_group), row);
}

/**
 * pt_asr_dialog_set_asr_manager:
 * @self: a `PtAsrDialog`
 * @asr_manager: a `PtAsrManager`
 *
 * Needs to be set before operating on configs.
 */
void
pt_asr_dialog_set_asr_manager (PtAsrDialog  *self,
                               PtAsrManager *asr_manager)
{
  g_return_if_fail (PT_IS_ASR_DIALOG (self));
  g_return_if_fail (PT_IS_ASR_MANAGER (asr_manager));

  self->asr_manager = asr_manager;
}

/**
 * pt_asr_dialog_set_config:
 * @self: a `PtAsrDialog`
 * @config: a `PtConfig`
 *
 * Populates the dialog with data from config.
 *
 * This is not intended to called more than once.
 */
void
pt_asr_dialog_set_config (PtAsrDialog *self,
                          PtConfig    *config)
{
  g_return_if_fail (config != NULL && PT_IS_CONFIG (config));

  self->config = config;
  GFile               *config_file;
  gchar               *active_path;
  gchar               *current_path;
  gchar               *name;
  gchar               *str;
  gchar               *engine = NULL;
  AdwPreferencesGroup *group;

  self->active = FALSE;
  active_path = g_settings_get_string (self->editor, "asr-config");
  if (active_path[0])
    {
      config_file = pt_config_get_file (config);
      current_path = g_file_get_path (config_file);
      self->active = g_strcmp0 (active_path, current_path) == 0;
      g_free (current_path);
    }
  g_free (active_path);

  self->installed = pt_config_is_installed (config);
  update_status_row (self);

  name = pt_config_get_name (config);
  adw_dialog_set_title (ADW_DIALOG (self), name);
  gtk_editable_set_text (GTK_EDITABLE (self->name_row), name);
  g_signal_connect (self->name_row, "apply",
                    G_CALLBACK (name_row_apply_cb), self);
  gtk_widget_set_focus_child (GTK_WIDGET (self), NULL);

  str = pt_config_get_lang_name (config);
  add_info_row (self, _ ("Language"), str);

  str = pt_config_get_plugin (config);
  if (g_strcmp0 (str, "parlasphinx") == 0)
    engine = _ ("CMU Pocketsphinx");

  if (engine)
    /* Translators: Model is a language model */
    add_info_row (self, _ ("Type of Model"), engine);
  else
    add_info_row (self, _ ("GStreamer Plugin"), str);

  str = pt_config_get_key (config, "Publisher");
  if (have_string (str))
    add_info_row (self, _ ("Publisher"), str);
  g_free (str);

  str = pt_config_get_key (config, "License");
  if (have_string (str))
    add_info_row (self, _ ("License"), str);
  g_free (str);

  /* Installation */
  group = ADW_PREFERENCES_GROUP (self->install_group);
  str = pt_config_get_key (config, "Howto");
  if (have_string (str))
    {
      PtPrefsInfoRow *howto_row = pt_prefs_info_row_new (_ ("Instructions"), str);
      adw_preferences_group_add (group, GTK_WIDGET (howto_row));
    }
  g_free (str);

  str = pt_config_get_key (config, "URL1");
  if (have_string (str))
    {
      GtkWidget *url1_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (url1_row), _ ("Download"));
      GtkWidget *link = pt_download_button_new (str);
      adw_action_row_add_suffix (ADW_ACTION_ROW (url1_row), link);
      adw_preferences_group_add (group, GTK_WIDGET (url1_row));
    }
  g_free (str);

  str = pt_config_get_key (config, "URL2");
  if (have_string (str))
    {
      GtkWidget *url2_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (url2_row), _ ("Download"));
      GtkWidget *link = pt_download_button_new (str);
      adw_action_row_add_suffix (ADW_ACTION_ROW (url2_row), link);
      adw_preferences_group_add (group, GTK_WIDGET (url2_row));
    }
  g_free (str);

  GtkWidget *install_row = pt_prefs_install_row_new (config);
  adw_preferences_group_add (group, install_row);
  g_signal_connect (install_row, "notify::installed", G_CALLBACK (installed_cb), self);

  if (!self->installed)
    {
      adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->page),
                                ADW_PREFERENCES_GROUP (self->install_group));
      adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->page),
                                ADW_PREFERENCES_GROUP (self->info_group));
    }
  else
    {
      adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->page),
                                ADW_PREFERENCES_GROUP (self->info_group));
      adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->page),
                                ADW_PREFERENCES_GROUP (self->install_group));
    }

  adw_preferences_page_add (ADW_PREFERENCES_PAGE (self->page),
                            ADW_PREFERENCES_GROUP (self->delete_group));
}

static void
add_toast_to_preferences_dialog (PtAsrDialog *self, AdwToast *toast)
{
  AdwApplicationWindow *win;
  AdwPreferencesDialog *dialog;
  GListModel           *dialogs;
  int                   position = 0;

  win = ADW_APPLICATION_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  dialogs = adw_application_window_get_dialogs (win);

  while (true)
    {
      dialog = g_list_model_get_item (dialogs, position);
      if (dialog == NULL)
        break;
      if (PT_IS_PREFERENCES_DIALOG (dialog))
        {
          adw_preferences_dialog_add_toast (ADW_PREFERENCES_DIALOG (dialog), toast);
          g_object_unref (dialog);
          break;
        }
      g_object_unref (dialog);
      position++;
    }

  g_object_unref (dialogs);
}

static void
file_delete_finished (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PtAsrDialog    *self = PT_ASR_DIALOG (user_data);
  GFile          *file = G_FILE (source_object);
  GError         *error = NULL;
  GtkAlertDialog *err_dialog;

  if (g_file_delete_finish (file, res, &error))
    {
      g_settings_set_string (self->editor, "asr-config", "");
      /* Translators: %s is replaced with the name of a configuration */
      AdwToast *toast = adw_toast_new_format (_ ("“%s” has been deleted"),
                                              pt_config_get_name (self->config));
      add_toast_to_preferences_dialog (self, toast);
      adw_dialog_close (ADW_DIALOG (self));
    }
  else
    {
      err_dialog = gtk_alert_dialog_new (_ ("Error"));
      gtk_alert_dialog_set_detail (err_dialog, error->message);
      gtk_alert_dialog_show (err_dialog, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))));
      g_object_unref (err_dialog);
      g_error_free (error);
    }
}

static void
delete_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  GFile       *file;

  file = pt_config_get_file (self->config);
  g_file_delete_async (file,
                       G_PRIORITY_DEFAULT,
                       NULL, /* cancellable */
                       file_delete_finished,
                       self);
}

static void
activate_button_clicked_cb (GtkButton *button,
                            gpointer   user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  gboolean     success;

  /* Depending on self->active this is either an "Activate" or a "Deactivate" button. */

  success = pt_asr_manager_set_config (self->asr_manager, self->active ? NULL : self->config);

  if (success)
    {
      /* Translators: %s is replaced with the name of a configuration */
      AdwToast *toast = adw_toast_new_format (_ ("“%s” has been activated"),
                                              pt_config_get_name (self->config));
      if (self->active)
        adw_toast_set_title (toast, _ ("Configuration has been deactivated"));
      add_toast_to_preferences_dialog (self, toast);
      adw_dialog_close (ADW_DIALOG (self));
    }
}

static void
cancel_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  g_cancellable_cancel (self->cancel);
}

static void
download_cb (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  PtAsrDialog    *self = PT_ASR_DIALOG (user_data);
  GError         *error = NULL;
  GtkAlertDialog *err_dialog;

  if (!pt_downloader_download_finish (PT_DOWNLOADER (source_object), res, &error))
    {
      err_dialog = gtk_alert_dialog_new (_ ("Error"));
      gtk_alert_dialog_set_detail (err_dialog, error->message);
      gtk_alert_dialog_show (err_dialog, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))));
      g_object_unref (err_dialog);
      g_error_free (error);
    }
}

static void
progress_cb (GObject *object,
             double   progress,
             gpointer user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), progress);
}

static void
download_button_clicked_cb (GtkButton *button,
                            gpointer   user_data)
{
  PtAsrDialog    *self = PT_ASR_DIALOG (user_data);
  PeasEngine     *engine;
  PeasPluginInfo *info;
  char           *downloader;

  engine = peas_engine_get_default ();
  downloader = pt_config_get_downloader (self->config);
  if (downloader == NULL)
    return;

  info = peas_engine_get_plugin_info (engine, downloader);
  peas_engine_load_plugin (engine, info);
  GObject *extension = peas_engine_create_extension (engine, info, PT_TYPE_DOWNLOADER, NULL);
  pt_config_configure_downloader (self->config, extension, NULL);
  self->cancel = g_cancellable_new ();
  pt_downloader_download_async (PT_DOWNLOADER (extension), self->cancel, download_cb, self);
  g_signal_connect (extension, "progress", G_CALLBACK (progress_cb), self);
}

static void
pt_asr_dialog_dispose (GObject *object)
{
  PtAsrDialog *self = PT_ASR_DIALOG (object);

  g_clear_object (&self->editor);
  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_ASR_DIALOG);

  G_OBJECT_CLASS (pt_asr_dialog_parent_class)->dispose (object);
}

static void
pt_asr_dialog_init (PtAsrDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->editor = g_settings_new (APP_ID);
  g_signal_connect (self->activate_button, "clicked",
                    G_CALLBACK (activate_button_clicked_cb), self);
  g_signal_connect (self->download_button, "clicked",
                    G_CALLBACK (download_button_clicked_cb), self);
  g_signal_connect (self->cancel_button, "clicked",
                    G_CALLBACK (cancel_button_clicked_cb), self);
}

static void
pt_asr_dialog_class_init (PtAsrDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_asr_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/asr-dialog.ui");
  gtk_widget_class_bind_template_callback (widget_class, delete_button_clicked_cb);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, page);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, status_row);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, name_row);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, info_group);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, install_group);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, delete_group);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, delete_row);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, activate_button);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, download_row);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, download_button);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, progress_bar);
}

/**
 * pt_asr_dialog_new:
 *
 * Returns a new dialog. Next set a config with pt_asr_dialog_set_config().
 *
 * Returns: a newly created `PtAsrDialog`.
 */
PtAsrDialog *
pt_asr_dialog_new (void)
{
  return g_object_new (PT_TYPE_ASR_DIALOG, NULL);
}
