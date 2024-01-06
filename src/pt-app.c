/* Copyright (C) Gabor Karsay 2016–2019 <gabor.karsay@gmx.at>
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

#include "pt-app.h"

#include "pt-dbus-service.h"
#include "pt-mpris.h"
#include "pt-preferences.h"
#include "pt-window.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

struct _PtApp
{
  AdwApplication parent;

  GSettings     *settings;
  PtMpris       *mpris;
  PtDbusService *dbus_service;
};

G_DEFINE_TYPE (PtApp, pt_app, ADW_TYPE_APPLICATION);

static GOptionEntry options[] = {
  { "version",
    'v',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE,
    NULL,
    N_ ("Show the application’s version"),
    NULL },
  { NULL }
};

static void
dialog_open_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  PtWindow *win = PT_WINDOW (user_data);
  GFile    *file;
  gchar    *uri = NULL;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, NULL);

  if (!file)
    return;

  uri = g_file_get_uri (file);
  g_object_unref (file);

  if (uri)
    {
      pt_window_open_file (win, uri);
      g_free (uri);
    }
}

static void
open_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  PtApp         *self = PT_APP (user_data);
  GtkFileDialog *dialog;
  GtkWindow     *parent;
  const char    *home_path;
  gchar         *current_uri = NULL;
  GFile         *current_file, *initial_folder;
  GtkFileFilter *filter_audio;
  GtkFileFilter *filter_all;
  GListStore    *filters;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, _ ("Open Audio File"));
  parent = gtk_application_get_active_window (GTK_APPLICATION (self));

  /* Set current folder to the folder with the currently open file
     or to user’s home directory */
  current_uri = pt_window_get_uri (PT_WINDOW (parent));
  if (current_uri)
    {
      current_file = g_file_new_for_uri (current_uri);
      initial_folder = g_file_get_parent (current_file);
      g_free (current_uri);
      g_object_unref (current_file);
    }
  else
    {
      home_path = g_get_home_dir ();
      initial_folder = g_file_new_for_path (home_path);
    }
  gtk_file_dialog_set_initial_folder (dialog, initial_folder);

  filter_audio = gtk_file_filter_new ();
  filter_all = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter_audio, _ ("Audio Files"));
  gtk_file_filter_set_name (filter_all, _ ("All Files"));
  gtk_file_filter_add_mime_type (filter_audio, "audio/*");
  gtk_file_filter_add_pattern (filter_all, "*");
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter_audio);
  g_list_store_append (filters, filter_all);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog, parent,
                        NULL, /* cancellable */
                        dialog_open_cb,
                        parent);

  g_object_unref (initial_folder);
  g_object_unref (dialog);
}

static void
prefs_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  PtApp     *app = PT_APP (user_data);
  GtkWindow *win;
  win = gtk_application_get_active_window (GTK_APPLICATION (app));
  pt_show_preferences_dialog (win);
}

static void
help_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  PtApp          *self = PT_APP (user_data);
  GtkUriLauncher *launcher;
  GtkWindow      *win;
  gchar          *uri;

  uri = g_strdup_printf ("help:%s", APP_ID);
  launcher = gtk_uri_launcher_new (uri);
  win = gtk_application_get_active_window (GTK_APPLICATION (self));
  gtk_uri_launcher_launch (launcher, win, NULL, NULL, NULL);
  g_free (uri);
  g_object_unref (launcher);
}

static void
about_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       app)
{
  AdwAboutWindow *about = ADW_ABOUT_WINDOW (adw_about_window_new ());

  const gchar *developers[] = {
    "Gabor Karsay <gabor.karsay@gmx.at>",
    NULL
  };

  const gchar *designers[] = {
    "GNOME Project http://www.gnome.org",
    "Gabor Karsay <gabor.karsay@gmx.at>",
    NULL
  };

  const gchar *credits_for_copy_paste_code[] = {
    "Buzztrax team <buzztrax-devel@buzztrax.org>",
    "Philip Withnall <philip@tecnocode.co.uk>",
    "Magnus Hjorth, mhWaveEdit",
    "Christian Hergert <chergert@redhat.com>",
    NULL
  };

  adw_about_window_add_credit_section (about, _ ("Code from other projects"), credits_for_copy_paste_code);

  adw_about_window_set_application_icon (about, APP_ID);
  adw_about_window_set_application_name (about, _ ("Parlatype"));
  adw_about_window_set_comments (about, _ ("A basic transcription utility"));
  adw_about_window_set_copyright (about, "© Gabor Karsay 2016–2024");
  adw_about_window_set_developers (about, developers);
  adw_about_window_set_developer_name (about, "Gabor Karsay");
  adw_about_window_set_designers (about, designers);
  adw_about_window_set_license_type (about, GTK_LICENSE_GPL_3_0);
  adw_about_window_set_translator_credits (about, _ ("translator-credits"));
  adw_about_window_set_version (about, PACKAGE_VERSION);
  adw_about_window_set_website (about, PACKAGE_URL);

  gtk_window_set_transient_for (GTK_WINDOW (about), gtk_application_get_active_window (app));
  gtk_window_present (GTK_WINDOW (about));
}

static void
quit_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  g_application_quit (G_APPLICATION (user_data));
}

const GActionEntry app_actions[] = {
  { "open", open_cb, NULL, NULL, NULL },
  { "prefs", prefs_cb, NULL, NULL, NULL },
  { "help", help_cb, NULL, NULL, NULL },
  { "about", about_cb, NULL, NULL, NULL },
  { "quit", quit_cb, NULL, NULL, NULL }
};

static gboolean
style_variant_to_color_scheme (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  const char *str = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (str, "follow") == 0)
    g_value_set_enum (value, ADW_COLOR_SCHEME_DEFAULT);
  else if (g_strcmp0 (str, "dark") == 0)
    g_value_set_enum (value, ADW_COLOR_SCHEME_FORCE_DARK);
  else
    g_value_set_enum (value, ADW_COLOR_SCHEME_FORCE_LIGHT);

  return TRUE;
}

static void
pt_app_startup (GApplication *app)
{
  G_APPLICATION_CLASS (pt_app_parent_class)->startup (app);
  PtApp *self = PT_APP (app);

  AdwStyleManager *style_manager;

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   app_actions, G_N_ELEMENTS (app_actions),
                                   self);

  const gchar *quit_accels[2] = { "<Primary>Q", NULL };
  const gchar *open_accels[2] = { "<Primary>O", NULL };
  const gchar *copy_accels[2] = { "<Primary>C", NULL };
  const gchar *insert_accels[2] = { "<Primary>V", NULL };
  const gchar *goto_accels[2] = { "<Primary>G", NULL };
  const gchar *help_accels[2] = { "F1", NULL };
  const gchar *zoom_in_accels[3] = { "<Primary>plus", "<Primary>KP_Add", NULL };
  const gchar *zoom_out_accels[3] = { "<Primary>minus", "<Primary>KP_Subtract", NULL };
  const char  *jump_back_accels[2] = { "<Primary>A", NULL };
  const char  *jump_forward_accels[2] = { "<Primary>S", NULL };
  const char  *play_accels[2] = { "<Primary>space", NULL };

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.quit", quit_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.open", open_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.copy", copy_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.insert", insert_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.goto", goto_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.help", help_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.zoom-in", zoom_in_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.zoom-out", zoom_out_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.jump-back", jump_back_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.jump-forward", jump_forward_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.play", play_accels);

  style_manager = adw_style_manager_get_default ();
  g_settings_bind_with_mapping (self->settings, "style-variant",
                                style_manager, "color-scheme",
                                G_SETTINGS_BIND_GET,
                                style_variant_to_color_scheme,
                                NULL, NULL, NULL);
}

static void
pt_app_activate (GApplication *application)
{
  PtApp    *self = PT_APP (application);
  GList    *windows;
  PtWindow *win;

  windows = gtk_application_get_windows (GTK_APPLICATION (application));
  if (windows)
    {
      win = PT_WINDOW (windows->data);
    }
  else
    {
      win = pt_window_new (self);
      self->mpris = pt_mpris_new (win);
      pt_mpris_start (self->mpris);
      self->dbus_service = pt_dbus_service_new (win);
      pt_dbus_service_start (self->dbus_service);
    }

  gtk_window_present (GTK_WINDOW (win));
}

static void
pt_app_open (GApplication *app,
             GFile       **files,
             gint          n_files,
             const gchar  *hint)
{
  GtkWindow *win;
  gchar     *uri;

  if (n_files > 1)
    {
      g_print (_ ("Warning: Parlatype handles only one file at a time. The other files are ignored.\n"));
    }

  uri = g_file_get_uri (files[0]);

  pt_app_activate (app);
  win = gtk_application_get_active_window (GTK_APPLICATION (app));
  pt_window_open_file (PT_WINDOW (win), uri);
  g_free (uri);
}

static gint
pt_app_handle_local_options (GApplication *application,
                             GVariantDict *options)
{
  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
      return 0;
    }

  return -1;
}

static void
pt_app_init (PtApp *self)
{
  self->settings = g_settings_new ("org.parlatype.Parlatype");
  self->mpris = NULL;
  self->dbus_service = NULL;
  g_application_add_main_option_entries (G_APPLICATION (self), options);
}

static void
pt_app_dispose (GObject *object)
{
  PtApp *self = PT_APP (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->mpris);
  g_clear_object (&self->dbus_service);

  G_OBJECT_CLASS (pt_app_parent_class)->dispose (object);
}

static void
pt_app_class_init (PtAppClass *klass)
{
  GApplicationClass *gapp_class = G_APPLICATION_CLASS (klass);
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = pt_app_dispose;
  gapp_class->open = pt_app_open;
  gapp_class->activate = pt_app_activate;
  gapp_class->startup = pt_app_startup;
  gapp_class->handle_local_options = pt_app_handle_local_options;
}

PtApp *
pt_app_new (void)
{
  return g_object_new (PT_APP_TYPE,
                       "application-id", APP_ID,
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
