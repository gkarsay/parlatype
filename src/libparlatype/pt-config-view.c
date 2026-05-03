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
 * PtConfigView:
 *
 * A dialog for a single ASR configuration.
 *
 * It presents most fields of a `PtConfig`, some of them can be changed, and
 * allows for activating, deactivating or deleting this config.
 */

#include "config.h"

#include "pt-config-view.h"

#include "pt-downloader.h"
#include "pt-preferences.h"
#include "pt-prefs-info-row.h"
#include "pt-prefs-install-row.h"
#include "pt-utils.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libpeas.h>
#include <parlatype.h>

struct _PtConfigView
{
  AdwBin parent_instance;

  PtConfig          *config;
  GSettings         *editor;
  PtAsrManager      *asr_manager;
  PtDownloadManager *download_manager;

  GtkBox        *page;
  AdwStatusPage *status_page;
  GtkWidget     *status_row;
  GtkWidget     *name_row;
  AdwActionRow  *language_row;
  AdwActionRow  *plugin_row;
  GtkWidget     *info_group;
  GtkWidget     *install_group;
  GtkWidget     *delete_group;
  GtkWidget     *delete_row;
  GtkWidget     *activate_button;
  GtkWidget     *download_row;
  GtkWidget     *download_button;

  gboolean active;
  gboolean installed;
};

enum
{
  PROP_0,
  PROP_CONFIG,
  PROP_ASR_MANAGER,
  PROP_DOWNLOAD_MANAGER,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtConfigView, pt_config_view, ADW_TYPE_BIN)

static void
name_row_apply_cb (GtkEditable *editable,
                   gpointer     user_data)
{
  PtConfigView *self = PT_CONFIG_VIEW (user_data);
  gchar        *old_name, *new_name;

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
update_download_row (PtConfigView *self)
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
update_status_row (PtConfigView *self)
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
  PtConfigView *self = PT_CONFIG_VIEW (user_data);
  gboolean      success;

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
add_info_row (PtConfigView *self,
              gchar        *title,
              gchar        *subtitle)
{
  GtkWidget *row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);
  gtk_widget_add_css_class (row, "property");
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->info_group), row);
}

/**
 * pt_config_view_set_asr_manager:
 * @self: a `PtConfigView`
 * @asr_manager: a `PtAsrManager`
 *
 * Needs to be set before operating on configs.
 */
void
pt_config_view_set_asr_manager (PtConfigView *self,
                                PtAsrManager *asr_manager)
{
  g_return_if_fail (PT_IS_CONFIG_VIEW (self));
  g_return_if_fail (PT_IS_ASR_MANAGER (asr_manager));

  self->asr_manager = asr_manager;
}

static void
add_toast_to_preferences_dialog (PtConfigView *self, AdwToast *toast)
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
  PtConfigView   *self = PT_CONFIG_VIEW (user_data);
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
  PtConfigView *self = PT_CONFIG_VIEW (user_data);
  GFile        *file;

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
  PtConfigView *self = PT_CONFIG_VIEW (user_data);
  gboolean      success;

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
download_button_clicked_cb (GtkButton *button,
                            gpointer   user_data)
{
  PtConfigView *self = PT_CONFIG_VIEW (user_data);
  g_print ("%s\n", __func__);
  pt_download_manager_download_async (self->download_manager, self->config, g_cancellable_new (), NULL, NULL);
}

static void
pt_config_view_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PtConfigView *self = PT_CONFIG_VIEW (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      g_clear_object (&self->config);
      self->config = g_value_dup_object (value);
      break;
    case PROP_ASR_MANAGER:
      self->asr_manager = g_value_get_object (value);
      break;
    case PROP_DOWNLOAD_MANAGER:
      self->download_manager = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_config_view_dispose (GObject *object)
{
  PtConfigView *self = PT_CONFIG_VIEW (object);

  g_clear_object (&self->config);
  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_CONFIG_VIEW);

  G_OBJECT_CLASS (pt_config_view_parent_class)->dispose (object);
}

static void
pt_config_view_constructed (GObject *object)
{
  PtConfigView *self = PT_CONFIG_VIEW (object);

  gchar               *name;
  gchar               *str;
  gchar               *downloader = NULL;
  AdwPreferencesGroup *group;

  g_object_bind_property (self->config, "name",
                          self->status_page, "title",
                          G_BINDING_SYNC_CREATE);

  self->active = FALSE;
  /* TODO compare with active config */

  self->installed = pt_config_is_installed (self->config);
  // update_status_row (self);

  if (!self->installed)
    gtk_widget_set_visible (self->status_row, FALSE);

  if (!self->installed)
    downloader = pt_config_get_downloader (self->config);

  if (downloader != NULL)
    {
      int   size = pt_config_get_download_size (self->config);
      char *size_string;
      if (size > 0)
        size_string = pt_utils_pretty_download_size (size);
      else
        size_string = "Unknown";
      adw_action_row_set_subtitle (ADW_ACTION_ROW (self->download_row), size_string);
    }
  else
    {
      gtk_widget_set_visible (self->download_row, FALSE);
    }

  name = pt_config_get_name (self->config);
  gtk_editable_set_text (GTK_EDITABLE (self->name_row), name);
  gtk_widget_set_focus_child (GTK_WIDGET (self), NULL);

  adw_action_row_set_subtitle (self->language_row,
                               pt_config_get_lang_name (self->config));

  adw_action_row_set_subtitle (self->plugin_row,
                               pt_config_get_plugin (self->config));

  str = pt_config_get_key (self->config, "Publisher");
  if (have_string (str))
    add_info_row (self, _ ("Publisher"), str);
  g_free (str);

  str = pt_config_get_key (self->config, "License");
  if (have_string (str))
    add_info_row (self, _ ("License"), str);
  g_free (str);

  /* Installation */
  group = ADW_PREFERENCES_GROUP (self->install_group);
  str = pt_config_get_key (self->config, "Howto");
  if (have_string (str))
    {
      PtPrefsInfoRow *howto_row = pt_prefs_info_row_new (_ ("Instructions"), str);
      adw_preferences_group_add (group, GTK_WIDGET (howto_row));
    }
  g_free (str);

  str = pt_config_get_key (self->config, "URL1");
  if (have_string (str))
    {
      GtkWidget *url1_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (url1_row), _ ("Download"));
      GtkWidget *link = pt_download_button_new (str);
      adw_action_row_add_suffix (ADW_ACTION_ROW (url1_row), link);
      adw_preferences_group_add (group, GTK_WIDGET (url1_row));
    }
  g_free (str);

  str = pt_config_get_key (self->config, "URL2");
  if (have_string (str))
    {
      GtkWidget *url2_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (url2_row), _ ("Download"));
      GtkWidget *link = pt_download_button_new (str);
      adw_action_row_add_suffix (ADW_ACTION_ROW (url2_row), link);
      adw_preferences_group_add (group, GTK_WIDGET (url2_row));
    }
  g_free (str);

  GtkWidget *install_row = pt_prefs_install_row_new (self->config);
  adw_preferences_group_add (group, install_row);
  g_signal_connect (install_row, "notify::installed", G_CALLBACK (installed_cb), self);

  G_OBJECT_CLASS (pt_config_view_parent_class)->constructed (object);
}

static void
pt_config_view_init (PtConfigView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->editor = g_settings_new (APP_ID);
}

static void
pt_config_view_class_init (PtConfigViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = pt_config_view_set_property;
  object_class->constructed = pt_config_view_constructed;
  object_class->dispose = pt_config_view_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/config-view.ui");
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, page);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, status_page);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, status_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, name_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, language_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, plugin_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, info_group);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, install_group);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, delete_group);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, delete_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, activate_button);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, download_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, download_button);
  gtk_widget_class_bind_template_callback (widget_class, name_row_apply_cb);
  gtk_widget_class_bind_template_callback (widget_class, activate_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, delete_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, download_button_clicked_cb);

  /**
   * PtConfigView:config:
   *
   * The configuration object.
   */
  obj_properties[PROP_CONFIG] =
      g_param_spec_object (
          "config", NULL, NULL,
          PT_TYPE_CONFIG,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfigView:asr-manager:
   *
   * A reference on a PtPlayer is needed, so that ASR plugins get audio input.
   */
  obj_properties[PROP_ASR_MANAGER] =
      g_param_spec_object (
          "asr-manager", NULL, NULL,
          PT_TYPE_ASR_MANAGER,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfigView:download-manager:
   *
   * A reference on a PtPlayer is needed, so that ASR plugins get audio input.
   */
  obj_properties[PROP_DOWNLOAD_MANAGER] =
      g_param_spec_object (
          "download-manager", NULL, NULL,
          PT_TYPE_DOWNLOAD_MANAGER,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_config_view_new:
 * @config: a `PtConfig`
 * @asr_manager: xxx
 * @download_manager: xxx
 *
 * Creates a `PtConfigView` for a particular config to be shown in `PtModelDialog`.
 *
 * Returns: a newly created `PtConfigView`.
 */
PtConfigView *
pt_config_view_new (PtConfig          *config,
                    PtAsrManager      *asr_manager,
                    PtDownloadManager *download_manager)
{
  g_return_val_if_fail (PT_IS_CONFIG (config), NULL);

  return g_object_new (PT_TYPE_CONFIG_VIEW,
                       "config", config,
                       "asr-manager", asr_manager,
                       "download-manager", download_manager,
                       NULL);
}
