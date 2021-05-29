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
#include "pt-app.h"


int main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE_NAME, LOCALEDIR);
	bind_textdomain_codeset (PACKAGE_NAME, "UTF-8");
	textdomain (PACKAGE_NAME);

	PtApp *app;
	gint   app_status;

	app = pt_app_new ();
	app_status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return app_status;
}
