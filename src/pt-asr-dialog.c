/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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
#include "pt-prefs-info-row.h"
#include "pt-prefs-install-row.h"
#include "pt-asr-dialog.h"


struct _PtAsrDialog
{
  AdwPreferencesWindow parent;

  PtConfig *config;
  GSettings *editor;

  GtkWidget *name_row;
  GtkWidget *info_group;
  GtkWidget *install_group;
  GtkWidget *activate_button;

  gboolean active;
};

G_DEFINE_FINAL_TYPE (PtAsrDialog, pt_asr_dialog, ADW_TYPE_PREFERENCES_WINDOW)

static void
name_row_apply_cb (GtkEditable *editable,
                   gpointer user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  gchar *old_name, *new_name;

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
      gtk_window_set_title (GTK_WINDOW (self), new_name);
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
static GtkWidget*
pt_download_button_new (gchar *link)
{
  GtkWidget *link_button;
  GUri *uri;
  const gchar *host = NULL;

  uri = g_uri_parse (link, G_URI_FLAGS_NONE, NULL);
  if (uri)
    {
      host = g_uri_get_host (uri);
    }

  link_button = gtk_link_button_new_with_label (link, host ? host : _("Link"));
  g_uri_unref (uri);
  return link_button;
}

static void
update_activation_button (PtAsrDialog *self)
{
  gtk_button_set_label (GTK_BUTTON (self->activate_button),
                        self->active ? _ ("Deactivate") : _ ("Activate"));
  gtk_widget_set_sensitive (self->activate_button,
                            pt_config_is_installed (self->config));
}

static void
installed_cb (PtPrefsInstallRow *row,
              GParamSpec *pspec,
              gpointer user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  gboolean installed, success;

  installed = pt_prefs_install_row_get_installed (row);

  if (!installed && self->active)
    {
      success = g_settings_set_string (self->editor, "asr-config", "");
      if (success)
        {
          self->active = FALSE;
        }
    }

  update_activation_button (self);
}

void
pt_asr_dialog_set_config (PtAsrDialog *self,
                          PtConfig *config)
{
  g_return_if_fail (config != NULL && PT_IS_CONFIG (config));

  self->config = config;
  GFile *config_file;
  gchar *active_path;
  gchar *current_path;
  gchar *name;
  gchar *str;
  gchar *engine = NULL;
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

  update_activation_button (self);

  group = ADW_PREFERENCES_GROUP (self->info_group);

  name = pt_config_get_name (config);
  gtk_window_set_title (GTK_WINDOW (self), name);
  gtk_editable_set_text (GTK_EDITABLE (self->name_row), name);
  g_signal_connect (self->name_row, "apply",
                    G_CALLBACK (name_row_apply_cb), self);
  gtk_widget_set_focus_child (GTK_WIDGET (self), NULL);

  str = pt_config_get_lang_name (config);
  PtPrefsInfoRow *lang_row = pt_prefs_info_row_new (_ ("Language"), str);
  adw_preferences_group_add (group, GTK_WIDGET (lang_row));

  str = pt_config_get_plugin (config);
  if (g_strcmp0 (str, "parlasphinx") == 0)
    engine = _ ("CMU Pocketsphinx");

  if (g_strcmp0 (str, "ptdeepspeech") == 0)
    engine = _ ("Mozilla DeepSpeech");
  PtPrefsInfoRow *engine_row;
  if (engine)
    engine_row = pt_prefs_info_row_new (_ ("Engine"), engine);
  else
    engine_row = pt_prefs_info_row_new (_ ("GStreamer Plugin"), str);

  adw_preferences_group_add (group, GTK_WIDGET (engine_row));

  str = pt_config_get_key (config, "Publisher");
  if (have_string (str))
    {
      PtPrefsInfoRow *publ_row = pt_prefs_info_row_new (_ ("Publisher"), str);
      adw_preferences_group_add (group, GTK_WIDGET (publ_row));
    }
  g_free (str);

  str = pt_config_get_key (config, "License");
  if (have_string (str))
    {
      PtPrefsInfoRow *license_row = pt_prefs_info_row_new (_ ("License"), str);
      adw_preferences_group_add (group, GTK_WIDGET (license_row));
    }
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
}

static void
file_delete_finished (GObject *source_object,
                      GAsyncResult *res,
                      gpointer user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  GFile *file = G_FILE (source_object);
  GError *error = NULL;
  GtkAlertDialog *err_dialog;
  GtkWindow *parent = GTK_WINDOW (self);

  if (g_file_delete_finish (file, res, &error))
    {
      gtk_window_close (GTK_WINDOW (self));
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
delete_dialog_cb (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  const gchar* response;
  GFile *file;

  response = adw_message_dialog_choose_finish (ADW_MESSAGE_DIALOG (source), result);
  if (!g_str_equal (response, "ok"))
    return;

  file = pt_config_get_file (self->config);
  g_file_delete_async (file,
                       G_PRIORITY_DEFAULT,
                       NULL, /* cancellable */
                       file_delete_finished,
                       self);
}

static void
delete_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  GtkWidget *dialog;

  dialog = adw_message_dialog_new (GTK_WINDOW (self),
                                   _ ("Delete configuration"),
                                   _ ("This will delete the configuration"));
  adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                    "cancel", _ ("_Cancel"),
                                    "ok", _ ("_OK"),
                                    NULL);
  adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog), "ok", ADW_RESPONSE_DESTRUCTIVE);
  adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "cancel");
  adw_message_dialog_set_close_response (ADW_MESSAGE_DIALOG (dialog), "cancel");

  adw_message_dialog_choose (ADW_MESSAGE_DIALOG (dialog),
                             NULL, /* cancellable */
                             delete_dialog_cb,
                             self);
}

static void
activate_button_clicked_cb (GtkButton *button,
                            gpointer   user_data)
{
  PtAsrDialog *self = PT_ASR_DIALOG (user_data);
  GFile *config_file;
  gchar *path;
  gboolean success;

  if (self->active)
    {
      path = g_strdup("");
    }
  else
    {
      if (!pt_config_is_installed (self->config))
        return;
      config_file = pt_config_get_file (self->config);
      path = g_file_get_path (config_file);
    }

  success = g_settings_set_string (self->editor, "asr-config", path);

  if (success)
    {
      self->active = !self->active;
      update_activation_button (self);
    }

  g_free (path);
}

static void
pt_asr_dialog_dispose (GObject *object)
{
  PtAsrDialog *self = PT_ASR_DIALOG (object);

  g_clear_object (&self->editor);

  G_OBJECT_CLASS (pt_asr_dialog_parent_class)->dispose (object);
}

static void
pt_asr_dialog_init (PtAsrDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->editor = g_settings_new (APP_ID);
  g_signal_connect (self->activate_button, "clicked",
                    G_CALLBACK (activate_button_clicked_cb), self);
}

static void
pt_asr_dialog_class_init (PtAsrDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_asr_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/asr-dialog.ui");
  gtk_widget_class_bind_template_callback (widget_class, delete_button_clicked_cb);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, name_row);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, info_group);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, install_group);
  gtk_widget_class_bind_template_child (widget_class, PtAsrDialog, activate_button);
}

PtAsrDialog *
pt_asr_dialog_new (GtkWindow *parent)
{
  return g_object_new (PT_TYPE_ASR_DIALOG, "transient-for", parent, NULL);
}
