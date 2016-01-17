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
#include <gtk/gtk.h>
#include <glib/gi18n.h>	
#include <stdlib.h>		/* exit() */
#include "pt-app.h"
#include "pt-player.h"

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
fatal_error_message (const gchar *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
			NULL,	/* this is discouraged, but we have no possible parent window */
			0,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			_("Fatal error"));

	gtk_message_dialog_format_secondary_text (
			GTK_MESSAGE_DIALOG (dialog),
                        _("Parlatype needs GStreamer 1.x to run. Please check your installation of "
                        "GStreamer and make sure you have the \"Good Plugins\" installed.\n"
                        "Parlatype will quit now, it received this error message: %s"), message);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

int main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
	
	GError         *error = NULL;
	GOptionContext *context;
	PtPlayer       *testplayer;

	/* Get command line options */
	context = g_option_context_new (_("[FILE]"));
	g_option_context_add_main_entries (context, options, PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_set_summary (context, _("Minimal audio player for manual speech transcription"));
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		return 1;
	}

	/* Test the backend */
	testplayer = pt_player_new (1.0, &error);
	if (error) {
		fatal_error_message (error->message);
		return 2;
	} else {
		g_object_unref (testplayer);
	}

	return g_application_run (G_APPLICATION (pt_app_new ()), argc, argv);
}

