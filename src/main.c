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
#include <locale.h>		/* setlocale */
#include <pt-player.h>
#include "pt-app.h"


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
	PtPlayer       *testplayer;
	PtApp          *app;
	gint            app_status;

	/* Test the backend */
	testplayer = pt_player_new (&error);
	if (error) {
		fatal_error_message (error->message);
		return 2;
	} else {
		g_object_unref (testplayer);
	}

	app = pt_app_new ();
	app_status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return app_status;
}
