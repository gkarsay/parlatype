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
  #include "pt-win32-helpers.h"
  #include "pt-win32-hotkeys.h"
  #include "pt-win32-pipe.h"
#endif
#include "pt-preferences.h"
#include "pt-window.h"
#include "pt-app.h"

struct _PtAppPrivate
{
#ifdef G_OS_UNIX
	PtMediakeys   *mediakeys;
	PtDbusService *dbus_service;
#endif
#ifdef G_OS_WIN32
	PtWin32Datacopy *win32datacopy;
	PtWin32Hotkeys  *win32hotkeys;
	PtWin32Pipe     *win32pipe;
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
	{ NULL }
};

static void
open_dialog_response_cb (GtkDialog *dialog,
			 gint       response_id,
			 gpointer   user_data)
{
	PtWindow *win = PT_WINDOW (user_data);
	gchar    *uri = NULL;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
	}

	if (uri) {
		pt_window_open_file (PT_WINDOW (win), uri);
		g_free (uri);
	}

	g_object_unref (dialog);
}

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
#ifdef G_OS_WIN32
	pt_win32_add_audio_patterns (filter_audio);
#else
	gtk_file_filter_add_mime_type (filter_audio, "audio/*");
#endif
	gtk_file_filter_add_pattern (filter_all, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_audio);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter_audio);

	g_signal_connect (dialog, "response", G_CALLBACK (open_dialog_response_cb), win);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));

	g_object_unref (dir);
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

#ifdef G_OS_WIN32
	uri = pt_win32_get_help_uri ();
#else
	uri = g_strdup_printf ("help:%s", APP_ID);
#endif

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
	const char *jump_back_accels[2] = {"<Primary>Left", NULL};
	const char *jump_forward_accels[2] = {"<Primary>Right", NULL};
	const char *play_accels[2] = {"<Primary>space", NULL};

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

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.jump-back", jump_back_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.jump-forward", jump_forward_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"win.play", play_accels);

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

#ifdef G_OS_WIN32
	if (pt_win32_present_other_instance (application))
		g_application_quit (application);
#endif

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
		app->priv->win32hotkeys = pt_win32_hotkeys_new (win);
		pt_win32_hotkeys_start (app->priv->win32hotkeys);
		app->priv->win32pipe = pt_win32_pipe_new (win);
		pt_win32_pipe_start (app->priv->win32pipe);
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

	if (n_files > 1) {
		g_print (_("Warning: Parlatype handles only one file at a time. The other files are ignored.\n"));
	}

	uri = g_file_get_uri (files[0]);

#ifdef G_OS_WIN32
	if (pt_win32_open_in_other_instance (app, uri)) {
		g_free (uri);
		g_application_quit (app);
	}
#endif

	pt_app_activate (app);
	win = gtk_application_get_active_window (GTK_APPLICATION (app));
	pt_window_open_file (PT_WINDOW (win), uri);
	g_free (uri);
}

static gint
pt_app_handle_local_options (GApplication *application,
                             GVariantDict *options)
{
	if (g_variant_dict_contains (options, "version")) {
		g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
		return 0;
	}

	return -1;
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
	app->priv->win32hotkeys = NULL;
	app->priv->win32pipe = NULL;
#endif
	g_application_add_main_option_entries (G_APPLICATION (app), options);
}

static void
pt_app_dispose (GObject *object)
{
	PtApp *app = PT_APP (object);

#ifdef G_OS_UNIX
	g_clear_object (&app->priv->mediakeys);
	g_clear_object (&app->priv->dbus_service);
#endif
#ifdef G_OS_WIN32
	g_clear_object (&app->priv->win32datacopy);
	g_clear_object (&app->priv->win32hotkeys);
	g_clear_object (&app->priv->win32pipe);
#endif

	G_OBJECT_CLASS (pt_app_parent_class)->dispose (object);
}

static void
pt_app_class_init (PtAppClass *klass)
{
	GApplicationClass    *gapp_class    = G_APPLICATION_CLASS (klass);
	GObjectClass         *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose           = pt_app_dispose;
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
