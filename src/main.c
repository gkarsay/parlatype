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

#include <girepository/girepository.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h> /* setlocale */

int
main (int argc, char *argv[])
{
  PtApp          *app;
  int             app_status;
  GOptionContext *context;
  GOptionGroup   *gir_group;

  /* Set up gettext translations */
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE_NAME, LOCALEDIR);
  bind_textdomain_codeset (PACKAGE_NAME, "UTF-8");
  textdomain (PACKAGE_NAME);

  /* Get option group for girepository, necessary to provide introspection information.
   * Do this early before GSettings schema is loaded because introspection is done
   * in the build phase before gschemas are installed. */
  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  gir_group = gi_repository_get_option_group ();
  g_option_context_add_group (context, g_steal_pointer (&gir_group));
  g_option_context_parse (context, &argc, &argv, NULL);
  g_option_context_free (context);

  app = pt_app_new ();
  g_application_add_option_group (G_APPLICATION (app), gi_repository_get_option_group ());
  app_status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return app_status;
}
