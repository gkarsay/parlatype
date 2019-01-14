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
#include <stdlib.h>		/* exit() */
#include <gtk/gtk.h>
#include <glib/gi18n.h>	
#include "pt-preferences.h"
#include "pt-window.h"
#include "pt-app.h"

struct _PtAppPrivate
{
	PtAsrSettings *asr_settings;
};


G_DEFINE_TYPE_WITH_PRIVATE (PtApp, pt_app, GTK_TYPE_APPLICATION);

static gboolean G_GNUC_NORETURN
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
	g_print ("%s %s\n", PACKAGE, VERSION);
	exit (0);
}

static GOptionEntry options[] =
{
	{ "version",
	  'v',
	  G_OPTION_FLAG_NO_ARG,
	  G_OPTION_ARG_CALLBACK,
	  option_version_cb,
	  N_("Show the application's version"),
	  NULL
	},
	{ NULL }
};

static void
open_cb (GSimpleAction *action,
	 GVariant      *parameter,
	 gpointer       app)
{
	GtkWindow     *win;
	const char    *home_path;
	gchar         *current_uri = NULL;
	GFile	      *current, *dir;
	GtkFileFilter *filter_audio;
	GtkFileFilter *filter_all;
	gchar         *uri = NULL;

	win = gtk_application_get_active_window (app);
#if GTK_CHECK_VERSION(3,20,0)
	GtkFileChooserNative *dialog;
	dialog = gtk_file_chooser_native_new (
			_("Open Audio File"),
			win,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("_Open"),
			_("_Cancel"));
#else
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new (
			_("Open Audio File"),
			win,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Open"), GTK_RESPONSE_ACCEPT,
			NULL);
#endif

	/* Set current folder to the folder with the currently open file
	   or to user's home directory */
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
	gtk_file_filter_add_mime_type (filter_audio, "audio/*");
	gtk_file_filter_add_pattern (filter_all, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_audio);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter_audio);

#if GTK_CHECK_VERSION(3,20,0)
	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
#else
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
#endif
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
	}

	g_object_unref (dir);
#if GTK_CHECK_VERSION(3,20,0)
	g_object_unref (dialog);
#else
	gtk_widget_destroy (dialog);
#endif
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

static void
help_cb (GSimpleAction *action,
	 GVariant      *parameter,
	 gpointer       app)
{
	GtkWindow *win;
	win = gtk_application_get_active_window (app);

	GError *error = NULL;
	gchar  *errmsg;

#if GTK_CHECK_VERSION(3,22,0)
	gtk_show_uri_on_window (
			win,
			"help:com.github.gkarsay.parlatype",
			GDK_CURRENT_TIME,
			&error);
#else
	gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (win)),
	              "help:com.github.gkarsay.parlatype",
	              GDK_CURRENT_TIME,
	              &error);
#endif

	if (error) {
		/* Translators: %s is a detailed error message */
		errmsg = g_strdup_printf (_("Error opening help: %s"), error->message);
		pt_error_message (PT_WINDOW (win), errmsg, NULL);
		g_free (errmsg);
		g_error_free (error);
	}
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
			"copyright", "© Gabor Karsay 2016–2018",
			"comments", _("A basic transcription utility"),
			"logo-icon-name", "com.github.gkarsay.parlatype",
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
	return app->priv->asr_settings;
}

static void
pt_app_startup (GApplication *app)
{
	g_application_set_resource_base_path (app, "/com/github/gkarsay/parlatype");
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
	gtk_css_provider_load_from_resource (provider, "/com/github/gkarsay/parlatype/parlatype.css");
	gtk_style_context_add_provider_for_screen (
			gdk_screen_get_default (),
			GTK_STYLE_PROVIDER (provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);
}

static void
pt_app_activate (GApplication *app)
{
	GList *windows;
	PtWindow *win;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	if (windows)
		win = PT_WINDOW (windows->data);
	else
		win = pt_window_new (PT_APP (app));

	gtk_window_present (GTK_WINDOW (win));
}

static void
pt_app_open (GApplication  *app,
	     GFile	  **files,
	     gint	    n_files,
	     const gchar   *hint)
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

static void
pt_app_init (PtApp *app)
{
	app->priv = pt_app_get_instance_private (app);

	g_application_add_main_option_entries (G_APPLICATION (app), options);

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

	G_OBJECT_CLASS (pt_app_parent_class)->dispose (object);
}

static void
pt_app_class_init (PtAppClass *klass)
{
	GApplicationClass    *gapp_class    = G_APPLICATION_CLASS (klass);
	GObjectClass         *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = pt_app_finalize;
	gapp_class->open        = pt_app_open;
	gapp_class->activate    = pt_app_activate;
	gapp_class->startup     = pt_app_startup;
}

PtApp *
pt_app_new (void)
{
	return g_object_new (PT_APP_TYPE,
			     "application-id", "com.github.gkarsay.parlatype",
			     "flags", G_APPLICATION_HANDLES_OPEN,
			     NULL);
}
