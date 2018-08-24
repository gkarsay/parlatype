/* Copyright (C) Gabor Karsay 2016 <gabor.karsay@gmx.at>
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
#include "pt-app.h"
#include "pt-window.h"
#include "pt-preferences.h"

struct _PtApp
{
	GtkApplication parent;
};

struct _PtAppClass
{
	GtkApplicationClass parent_class;
};

G_DEFINE_TYPE (PtApp, pt_app, GTK_TYPE_APPLICATION);

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
recent_cb (GSimpleAction *action,
	   GVariant      *parameter,
	   gpointer       app)
{
	GtkWidget	 *dialog;
	GtkWindow	 *win;
	GtkRecentInfo	 *info = NULL;
	GtkRecentFilter  *filter;
	GtkRecentFilter  *filter_all;
	const gchar	 *uri = NULL;

	win = gtk_application_get_active_window (app);
	dialog = gtk_recent_chooser_dialog_new (
			_("Recent Files"),
			win,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Open"), GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (dialog), GTK_RECENT_SORT_MRU); /* Most Recently Used first */
	gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER (dialog), TRUE);
	filter = gtk_recent_filter_new ();
	gtk_recent_filter_set_name (filter, _("Parlatype"));
	gtk_recent_filter_add_application (filter, "parlatype");
	gtk_recent_filter_add_mime_type (filter, "audio/*");
	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (dialog), filter);

	filter_all = gtk_recent_filter_new ();
	gtk_recent_filter_set_name (filter_all, _("All files"));
	gtk_recent_filter_add_pattern (filter_all, "*");
	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (dialog), filter_all);

	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (dialog), filter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		info = gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER (dialog));
		if (info)
			uri = gtk_recent_info_get_uri (info);
	}

	gtk_widget_destroy (dialog);

	if (uri) {
		/* GStreamer has problems with uris from recent chooser,
		   as a workaround use GFile magic */
		GFile *file = g_file_new_for_uri (uri);
		gchar *tmp = g_file_get_uri (file);
		pt_window_open_file (PT_WINDOW (win), tmp);
		g_free (tmp);
		g_object_unref (file);
		gtk_recent_info_unref (info);
	}
}

static void
open_cb (GSimpleAction *action,
	 GVariant      *parameter,
	 gpointer       app)
{
	GtkWidget     *dialog;
	GtkWindow     *win;
	const char    *home_path;
	GFile	      *home;
	GtkFileFilter *filter_audio;
	GtkFileFilter *filter_all;
	gchar         *uri = NULL;

	win = gtk_application_get_active_window (app);
	dialog = gtk_file_chooser_dialog_new (
			_("Open Audio File"),
			win,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Open"), GTK_RESPONSE_ACCEPT,
			NULL);

	/* Set current folder to user's home directory */
	home_path = g_get_home_dir ();
	home = g_file_new_for_path (home_path);
	gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (dialog), home, NULL);

	filter_audio = gtk_file_filter_new ();
	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_audio, _("Audio files"));
	gtk_file_filter_set_name (filter_all, _("All files"));
	gtk_file_filter_add_mime_type (filter_audio, "audio/*");
	gtk_file_filter_add_pattern (filter_all, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_audio);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter_audio);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
	}

	g_object_unref (home);
	gtk_widget_destroy (dialog);

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
		pt_error_message (PT_WINDOW (win), errmsg);
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
	{ "recent", recent_cb, NULL, NULL, NULL },
	{ "prefs", prefs_cb, NULL, NULL, NULL },
	{ "help", help_cb, NULL, NULL, NULL },
	{ "about", about_cb, NULL, NULL, NULL },
	{ "quit", quit_cb, NULL, NULL, NULL }
};

static void
pt_app_startup (GApplication *app)
{
	GtkBuilder *builder;
	GMenuModel *app_menu;
	GMenuModel *menubar;

#if GLIB_CHECK_VERSION(2,42,0)
	g_application_set_resource_base_path (app, "/com/github/gkarsay/parlatype");
#endif
	G_APPLICATION_CLASS (pt_app_parent_class)->startup (app);

	g_action_map_add_action_entries (G_ACTION_MAP (app),
	                                 app_actions, G_N_ELEMENTS (app_actions),
	                                 app);

	const gchar *quit_accels[2] = { "<Primary>Q", NULL };
	const gchar *open_accels[2] = { "<Primary>O", NULL };
	const gchar *copy_accels[2] = { "<Primary>C", NULL };
	const gchar *insert_accels[2] = { "<Primary>V", NULL };
	const gchar *goto_accels[2] = { "<Primary>G", NULL };
	const gchar *recent_accels[2] = { "<Primary>R", NULL };
	const gchar *help_accels[2] = { "F1", NULL };
	const gchar *zoom_in_accels[3] = { "<Primary>plus", "<Primary>KP_Add", NULL };
	const gchar *zoom_out_accels[3] = { "<Primary>minus", "<Primary>KP_Subtract", NULL };

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"app.quit", quit_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"app.open", open_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (app),
			"app.recent", recent_accels);

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

	builder = gtk_builder_new_from_resource ("/com/github/gkarsay/parlatype/menus.ui");

	if (gtk_application_prefers_app_menu (GTK_APPLICATION (app))) {
#if GTK_CHECK_VERSION(3,20,0)
		app_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));
#else
		app_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu-pre-3-20"));
#endif
		gtk_application_set_app_menu (GTK_APPLICATION (app), app_menu);
	} else {
#if GTK_CHECK_VERSION(3,20,0)
		menubar = G_MENU_MODEL (gtk_builder_get_object (builder, "alternative-menu"));
#else
		menubar = G_MENU_MODEL (gtk_builder_get_object (builder, "alternative-menu-pre-3-20"));
#endif
		gtk_application_set_menubar (GTK_APPLICATION (app), menubar);
	}

	g_object_unref (builder);

	/* Load custom css */
	GtkCssProvider *provider;
	GFile          *file;
	file = g_file_new_for_uri ("resource:///com/github/gkarsay/parlatype/parlatype.css");
	provider = gtk_css_provider_new ();
	/* Note: since 3.16 there's also gtk_css_provider_load_from_resource () */
	gtk_css_provider_load_from_file (provider, file, NULL);
	gtk_style_context_add_provider_for_screen (
			gdk_screen_get_default (),
			GTK_STYLE_PROVIDER (provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);
	g_object_unref (file);
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

static void
pt_app_class_init (PtAppClass *class)
{
	G_APPLICATION_CLASS (class)->open = pt_app_open;
	G_APPLICATION_CLASS (class)->activate = pt_app_activate;
	G_APPLICATION_CLASS (class)->startup = pt_app_startup;
}

static void
pt_app_init (PtApp *app)
{
	g_application_add_main_option_entries (G_APPLICATION (app), options);
}

PtApp *
pt_app_new (void)
{
	return g_object_new (PT_APP_TYPE,
			     "application-id", "com.github.gkarsay.parlatype",
			     "flags", G_APPLICATION_HANDLES_OPEN,
			     NULL);
}
