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
#include <gtk/gtk.h>
#include <glib/gi18n.h>	
#ifdef G_OS_UNIX
  #include "pt-dbus-service.h"
  #include "pt-mediakeys.h"
#endif
#ifdef G_OS_WIN32
  #include "pt-win32-datacopy.h"
#endif
#include "pt-preferences.h"
#include "pt-window.h"
#include "pt-app.h"

struct _PtAppPrivate
{
	gboolean       asr;
	gboolean       atspi;
	PtAsrSettings *asr_settings;
#ifdef G_OS_UNIX
	PtMediakeys   *mediakeys;
	PtDbusService *dbus_service;
#endif
#ifdef G_OS_WIN32
	PtWin32Datacopy *win32datacopy;
#endif
};


G_DEFINE_TYPE_WITH_PRIVATE (PtApp, pt_app, GTK_TYPE_APPLICATION);


static GOptionEntry options[] =
{
	{ "version",
	  'v',
	  G_OPTION_FLAG_NONE,
	  G_OPTION_ARG_NONE,
	  NULL,
	  N_("Show the application’s version"),
	  NULL
	},
#ifdef HAVE_ASR
	{ "with-asr",
	  'a',
	  G_OPTION_FLAG_NONE,
	  G_OPTION_ARG_NONE,
	  NULL,
	  N_("Enable automatic speech recognition"),
	  NULL
	},
	{ "textpad",
	  't',
	  G_OPTION_FLAG_NONE,
	  G_OPTION_ARG_NONE,
	  NULL,
	  N_("Use internal textpad for automatic speech recognition"),
	  NULL
	},
#endif
	{ NULL }
};

static void
open_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       app)
{
	GtkFileChooserNative *dialog;
	GtkWindow     *win;
	const char    *home_path;
	gchar         *current_uri = NULL;
	GFile	      *current, *dir;
	GtkFileFilter *filter_audio;
	GtkFileFilter *filter_all;
	gchar         *uri = NULL;

	win = gtk_application_get_active_window (app);
	dialog = gtk_file_chooser_native_new (
			_("Open Audio File"),
			win,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("_Open"),
			_("_Cancel"));

	/* Set current folder to the folder with the currently open file
	   or to user’s home directory */
	current_uri = pt_window_get_uri (PT_WINDOW (win));
	if (current_uri) {
		current = g_file_new_for_uri (current_uri);
		dir = g_file_get_parent (current);
		g_free (current_uri);
		g_object_unref (current);
	} else {
		home_path = g_get_home_dir ();
		dir = g_file_new_for_path (home_path);
	}
	gtk_file_chooser_set_current_folder_file (
		GTK_FILE_CHOOSER (dialog), dir, NULL);

	filter_audio = gtk_file_filter_new ();
	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_audio, _("Audio files"));
	gtk_file_filter_set_name (filter_all, _("All files"));
#ifdef GDK_WINDOWING_WIN32
	gtk_file_filter_add_pattern (filter_audio, "*.aac");
	gtk_file_filter_add_pattern (filter_audio, "*.aif");
	gtk_file_filter_add_pattern (filter_audio, "*.aiff");
	gtk_file_filter_add_pattern (filter_audio, "*.flac");
	gtk_file_filter_add_pattern (filter_audio, "*.iff");
	gtk_file_filter_add_pattern (filter_audio, "*.mpa");
	gtk_file_filter_add_pattern (filter_audio, "*.mp3");
	gtk_file_filter_add_pattern (filter_audio, "*.m4a");
	gtk_file_filter_add_pattern (filter_audio, "*.oga");
	gtk_file_filter_add_pattern (filter_audio, "*.ogg");
	gtk_file_filter_add_pattern (filter_audio, "*.wav");
	gtk_file_filter_add_pattern (filter_audio, "*.wma");
#else
	gtk_file_filter_add_mime_type (filter_audio, "audio/*");
#endif
	gtk_file_filter_add_pattern (filter_all, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_audio);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter_audio);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
	}

	g_object_unref (dir);
	g_object_unref (dialog);

	if (uri) {
		pt_window_open_file (PT_WINDOW (win), uri);
		g_free (uri);
	}
}

static void
prefs_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       app)
{
	GtkWindow *win;
	win = gtk_application_get_active_window (app);
	pt_show_preferences_dialog (win);
}

static gchar*
get_help_uri (void)
{
	gchar *uri;

#ifdef GDK_WINDOWING_WIN32

	gchar      *exe_path;
	gchar      *help_path;
	GRegex     *r_long;
	GRegex     *r_short;
	GMatchInfo *match;
	gchar      *l_raw;
	gchar      *l_long;
	gchar      *l_short;
	gchar      *p_long;
	gchar      *p_short;
	gchar      *path;
	GFile      *file;

	exe_path = g_win32_get_package_installation_directory_of_module (NULL);
	help_path = g_build_filename(exe_path, "share", "help", NULL);
	l_raw = g_win32_getlocale ();
	r_long = g_regex_new ("^[A-Z]*_[A-Z]*", G_REGEX_CASELESS, 0, NULL);
	r_short = g_regex_new ("^[A-Z]*", G_REGEX_CASELESS, 0, NULL);

	g_regex_match (r_long, l_raw, 0, &match);
	if (g_match_info_matches (match)) {
		l_long = g_match_info_fetch (match, 0);
		p_long = g_build_filename (help_path, l_long, APP_ID, "index.html", NULL);
	}
	g_match_info_free (match);

	g_regex_match (r_short, l_raw, 0, &match);
	if (g_match_info_matches (match)) {
		l_short = g_match_info_fetch (match, 0);
		p_short = g_build_filename (help_path, l_short, APP_ID, "index.html", NULL);
	}
	g_match_info_free (match);

	if (g_file_test (p_long, G_FILE_TEST_EXISTS))
		path = g_strdup (p_long);
	else if (g_file_test (p_short, G_FILE_TEST_EXISTS))
		path = g_strdup (p_short);
	else path = g_build_filename (help_path, "C", APP_ID, "index.html", NULL);

	file = g_file_new_for_path (path);
	uri = g_file_get_uri (file);

	g_object_unref (file);
	g_free (path);
	g_free (p_long);
	g_free (p_short);
	g_free (l_long);
	g_free (l_short);
	g_free (l_raw);
	g_regex_unref (r_long);
	g_regex_unref (r_short);
	g_free (help_path);
	g_free (exe_path);
#else
	uri = g_strdup_printf ("help:%s", APP_ID);
#endif
	return uri;
}

static void
help_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       app)
{
	GtkWindow *win;
	gchar     *uri;
	GError    *error = NULL;
	gchar     *errmsg;

	win = gtk_application_get_active_window (app);
	uri = get_help_uri ();

	gtk_show_uri_on_window (
			win,
			uri,
			GDK_CURRENT_TIME,
			&error);

	if (error) {
		/* Translators: %s is a detailed error message */
		errmsg = g_strdup_printf (_("Error opening help: %s"), error->message);
		pt_error_message (PT_WINDOW (win), errmsg, NULL);
		g_free (errmsg);
		g_error_free (error);
	}

	g_free (uri);
}

static void
about_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       app)
{
	const gchar *authors[] = {
		"Gabor Karsay <gabor.karsay@gmx.at>",
		/* Translators: This is part of the about box, followed by the
		   copyright holders of code from other projects. Instead of
		   "including" it could also say "with" or just "Code from
		   other projects:" */
		_("Including code from other projects:"),
		"Buzztrax team <buzztrax-devel@buzztrax.org>",
		"Philip Withnall <philip@tecnocode.co.uk>",
		"Magnus Hjorth, mhWaveEdit",
		NULL
	};

	const gchar *artists[] = {
		"GNOME Project http://www.gnome.org",
		"Gabor Karsay <gabor.karsay@gmx.at>",
		NULL
	};

	gtk_show_about_dialog (
			gtk_application_get_active_window (app),
			"program_name", _("Parlatype"),
			"version", PACKAGE_VERSION,
			"copyright", "© Gabor Karsay 2016–2020",
			"comments", _("A basic transcription utility"),
			"logo-icon-name", APP_ID,
			"authors", authors,
			"artists", artists,
			"translator-credits", _("translator-credits"),
			"license-type", GTK_LICENSE_GPL_3_0,
			"website", PACKAGE_URL,
			NULL);
}

static void
quit_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       app)
{
	g_application_quit (app);
}

const GActionEntry app_actions[] = {
	{ "open", open_cb, NULL, NULL, NULL },
	{ "prefs", prefs_cb, NULL, NULL, NULL },
	{ "help", help_cb, NULL, NULL, NULL },
	{ "about", about_cb, NULL, NULL, NULL },
	{ "quit", quit_cb, NULL, NULL, NULL }
};

PtAsrSettings*
pt_app_get_asr_settings (PtApp *app)
{
	g_assert (PT_IS_APP (app));
	return app->priv->asr_settings;
}

gboolean
pt_app_get_atspi (PtApp *app)
{
	g_assert (PT_IS_APP (app));
	return app->priv->atspi;
}

gboolean
pt_app_get_asr (PtApp *app)
{
	g_assert (PT_IS_APP (app));
	return app->priv->asr;
}

static void
pt_app_startup (GApplication *app)
{
	g_application_set_resource_base_path (app, "/org/parlatype/parlatype");
	G_APPLICATION_CLASS (pt_app_parent_class)->startup (app);

	g_action_map_add_action_entries (G_ACTION_MAP (app),
	                                 app_actions, G_N_ELEMENTS (app_actions),
	                                 app);

	const gchar *quit_accels[2] = { "<Primary>Q", NULL };
	const gchar *open_accels[2] = { "<Primary>O", NULL };
	const gchar *copy_accels[2] = { "<Primary>C", NULL };
	const gchar *insert_accels[2] = { "<Primary>V", NULL };
	const gchar *goto_accels[2] = { "<Primary>G", NULL };
	const gchar *help_accels[2] = { "F1", NULL };
	const gchar *zoom_in_accels[3] = { "<Primary>plus", "<Primary>KP_Add", NULL };
	const gchar *zoom_out_accels[3] = { "<Primary>minus", "<Primary>KP_Subtract", NULL };

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"app.quit", quit_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"app.open", open_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.copy", copy_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.insert", insert_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.goto", goto_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"app.help", help_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.zoom-in", zoom_in_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.zoom-out", zoom_out_accels);

	/* Load custom css */
	GtkCssProvider *provider;
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_resource (provider, "/org/parlatype/parlatype/parlatype.css");
	gtk_style_context_add_provider_for_screen (
			gdk_screen_get_default (),
			GTK_STYLE_PROVIDER (provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);
}

static void
pt_app_activate (GApplication *application)
{
	PtApp    *app = PT_APP (application);
	GList    *windows;
	PtWindow *win;

	windows = gtk_application_get_windows (GTK_APPLICATION (application));
	if (windows) {
		win = PT_WINDOW (windows->data);
	} else {
		win = pt_window_new (app);
#ifdef G_OS_UNIX
		app->priv->mediakeys = pt_mediakeys_new (win);
		pt_mediakeys_start (app->priv->mediakeys);
		app->priv->dbus_service = pt_dbus_service_new (win);
		pt_dbus_service_start (app->priv->dbus_service);
#endif
#ifdef G_OS_WIN32
		app->priv->win32datacopy = pt_win32_datacopy_new (win);
		pt_win32_datacopy_start (app->priv->win32datacopy);
#endif
	}

	gtk_window_present (GTK_WINDOW (win));
}

static void
pt_app_open (GApplication  *app,
             GFile       **files,
             gint          n_files,
             const gchar  *hint)
{
	GtkWindow *win;
	gchar	  *uri;

	pt_app_activate (app);
	win = gtk_application_get_active_window (GTK_APPLICATION (app));

	if (n_files > 1) {
		g_print (_("Warning: Parlatype handles only one file at a time. The other files are ignored.\n"));
	}

	uri = g_file_get_uri (files[0]);
	pt_window_open_file (PT_WINDOW (win), uri);
	g_free (uri);
}

static gint
pt_app_handle_local_options (GApplication *application,
                             GVariantDict *options)
{
	PtApp *app = PT_APP (application);

	if (g_variant_dict_contains (options, "version")) {
		g_print ("%s %s\n", PACKAGE, VERSION);
		return 0;
	}

	if (g_variant_dict_contains (options, "with-asr")) {
		app->priv->asr = TRUE;
	}

	if (g_variant_dict_contains (options, "textpad")) {
		app->priv->atspi = FALSE;
	}

	return -1;
}

static gchar*
get_asr_settings_filename (void)
{
	const gchar *userdir;
	gchar	    *configdir;
	GFile	    *create_dir;
	gchar       *filename;

	userdir = g_get_user_data_dir ();
	configdir = g_build_path ("/", userdir, PACKAGE, NULL);
	create_dir = g_file_new_for_path (configdir);
	g_file_make_directory (create_dir, NULL, NULL);
	filename = g_build_filename (configdir, "asr.ini", NULL);

	g_free (configdir);
	g_object_unref (create_dir);

	return filename;
}

static gboolean
is_flatpak (void)
{
	/* Flatpak sandboxes have a file ".flatpak-info" in their filesystem root.
	   It's a keyfile, an example how to read is get_flatpak_information() in
	   https://gitlab.gnome.org/GNOME/recipes/blob/master/src/gr-about-dialog.c */

	return (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS));
}

static void
pt_app_init (PtApp *app)
{
	app->priv = pt_app_get_instance_private (app);

#ifdef G_OS_UNIX
	app->priv->mediakeys = NULL;
	app->priv->dbus_service = NULL;
#endif
#ifdef G_OS_WIN32
	app->priv->win32datacopy = NULL;
#endif
	g_application_add_main_option_entries (G_APPLICATION (app), options);

	/* In Flatpak's sandbox ATSPI doesn't work */
	if (is_flatpak ())
		app->priv->atspi = FALSE;
	else
		app->priv->atspi = TRUE;

	app->priv->asr = FALSE;
	gchar *asr_settings_filename;
	asr_settings_filename = get_asr_settings_filename ();
	app->priv->asr_settings = pt_asr_settings_new (asr_settings_filename);
	g_free (asr_settings_filename);
}

static void
pt_app_finalize (GObject *object)
{
	PtApp *app = PT_APP (object);

	g_clear_object (&app->priv->asr_settings);
#ifdef G_OS_UNIX
	g_clear_object (&app->priv->mediakeys);
	g_clear_object (&app->priv->dbus_service);
#endif
#ifdef G_OS_WIN32
	g_clear_object (&app->priv->win32datacopy);
#endif

	G_OBJECT_CLASS (pt_app_parent_class)->dispose (object);
}

static void
pt_app_class_init (PtAppClass *klass)
{
	GApplicationClass    *gapp_class    = G_APPLICATION_CLASS (klass);
	GObjectClass         *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize          = pt_app_finalize;
	gapp_class->open                 = pt_app_open;
	gapp_class->activate             = pt_app_activate;
	gapp_class->startup              = pt_app_startup;
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
