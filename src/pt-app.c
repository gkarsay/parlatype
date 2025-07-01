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

#include "config.h"

#include "pt-app.h"

#include "pt-dbus-service.h"
#include "pt-mpris.h"
#include "pt-preferences.h"
#include "pt-window-private.h"
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
  GtkApplication      *app = GTK_APPLICATION (user_data);
  GtkWindow           *parent = gtk_application_get_active_window (app);
  PtPreferencesDialog *dlg = pt_preferences_dialog_new ();
  adw_dialog_present (ADW_DIALOG (dlg), GTK_WIDGET (parent));
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

static gchar *
get_os_info (void)
{
  gchar *pretty_name;
  gchar *name;
  gchar *version_id;
  gchar *result;

  pretty_name = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);
  if (pretty_name)
    return pretty_name;

  name = g_get_os_info (G_OS_INFO_KEY_NAME);
  version_id = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);

  if (name && version_id)
    result = g_strdup_printf ("%s %s", name, version_id);
  else
    result = g_strdup ("unknown");

  g_free (name);
  g_free (version_id);
  return result;
}

static char *
get_debug_info (GtkApplication *app)
{
  GString     *str = g_string_new (NULL);
  GtkSettings *gtk_settings = gtk_settings_get_default ();
  gchar       *theme_name;
  gchar       *os_info;
  PtWindow    *win;
  PtPlayer    *player;
  PtMediaInfo *info;
  gchar       *uri;
  gchar       *gst_caps;
  gchar       *gst_tags;

  win = PT_WINDOW (gtk_application_get_active_window (app));
  player = _pt_window_get_player (win);

  g_string_append (str, "[version]\n");

  g_string_append_printf (str, "Parlatype: %s\n", PACKAGE_VERSION);
  g_string_append_printf (str,
                          "GLib: %d.%d.%d (compiled with %d.%d.%d)\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version,
                          GLIB_MAJOR_VERSION,
                          GLIB_MINOR_VERSION,
                          GLIB_MICRO_VERSION);
  g_string_append_printf (str,
                          "GTK: %d.%d.%d (compiled with %d.%d.%d)\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version (),
                          GTK_MAJOR_VERSION,
                          GTK_MINOR_VERSION,
                          GTK_MICRO_VERSION);
  g_string_append_printf (str,
                          "Libadwaita: %d.%d.%d (compiled with %d.%d.%d)\n",
                          adw_get_major_version (),
                          adw_get_minor_version (),
                          adw_get_micro_version (),
                          ADW_MAJOR_VERSION,
                          ADW_MINOR_VERSION,
                          ADW_MICRO_VERSION);

  g_string_append (str, "\n[environment]\n");

  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    g_string_append (str, "Flatpak: yes\n");

  os_info = get_os_info ();
  g_string_append_printf (str, "OS: %s\n", os_info);
  g_free (os_info);

  g_string_append_printf (str, "XDG_CURRENT_DESKTOP: %s\n", g_getenv ("XDG_CURRENT_DESKTOP") ?: "unset");
  g_string_append_printf (str, "GdkDisplay: %s\n", G_OBJECT_TYPE_NAME (gdk_display_get_default ()));

  g_object_get (gtk_settings, "gtk-theme-name", &theme_name, NULL);
  g_string_append_printf (str, "gtk-theme-name: %s\n", theme_name);
  g_free (theme_name);

  g_string_append_printf (str, "GTK_THEME: %s\n", g_getenv ("GTK_THEME") ?: "unset");

  uri = pt_player_get_uri (player);
  if (uri != NULL)
    {
      g_string_append (str, "\n[current media]\n");
      g_string_append_printf (str, "uri: %s\n", uri);
      g_free (uri);

      info = pt_player_get_media_info (player);
      gst_caps = pt_media_info_dump_caps (info);
      g_string_append (str, gst_caps);
      g_free (gst_caps);

      gst_tags = pt_media_info_dump_tags (info);
      g_string_append (str, gst_tags);
      g_free (gst_tags);
    }

#ifdef HAVE_ASR
  g_string_append (str, "\n[automatic speech recognition]\n");
  gchar *asr_path = g_settings_get_string (PT_APP (app)->settings, "asr-config");
  g_string_append_printf (str, "asr-config: %s\n", asr_path);
  g_free (asr_path);
#endif

  return g_string_free_and_steal (str);
}

static void
about_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  GtkApplication *app = GTK_APPLICATION (user_data);
  GtkWindow      *parent = gtk_application_get_active_window (app);
  AdwAboutDialog *about = ADW_ABOUT_DIALOG (adw_about_dialog_new ());
  gchar          *debug_info = get_debug_info (app);

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
    /* roughly in chronological order */
    "Frederik Elwert (Transcribe)",
    "Buzztrax team (Buzztrax)",
    "Magnus Hjorth (mhWaveEdit)",
    "Philip Withnall (totem)",
    "David Huggins-Daines (pocketsphinx)",
    "Jonathan Matthew (Rhythmbox)",
    "William Jon McCann, Ray Strode (gnome-desktop)",
    "Christian Hergert (gnome-text-editor)",
    NULL
  };

  adw_about_dialog_add_credit_section (about, _ ("Code from other projects"), credits_for_copy_paste_code);

  adw_about_dialog_set_application_icon (about, APP_ID);
  adw_about_dialog_set_application_name (about, _ ("Parlatype"));
  adw_about_dialog_set_comments (about, _ ("A basic transcription utility"));
  adw_about_dialog_set_copyright (about, "© Gabor Karsay 2016–2025");
  adw_about_dialog_set_debug_info (about, debug_info);
  adw_about_dialog_set_developers (about, developers);
  adw_about_dialog_set_developer_name (about, "Gabor Karsay");
  adw_about_dialog_set_designers (about, designers);
  adw_about_dialog_set_license_type (about, GTK_LICENSE_GPL_3_0);
  adw_about_dialog_set_translator_credits (about, _ ("translator-credits"));
  adw_about_dialog_set_version (about, PACKAGE_VERSION);
  adw_about_dialog_set_website (about, PACKAGE_URL);

  adw_dialog_present (ADW_DIALOG (about), GTK_WIDGET (parent));

  g_free (debug_info);
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
             int           n_files,
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

static int
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
  self->settings = g_settings_new ("xyz.parlatype.Parlatype");
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
  return g_object_new (PT_TYPE_APP,
                       "application-id", APP_ID,
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
