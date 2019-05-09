/* Copyright (C) Gabor Karsay 2019 <gabor.karsay@gmx.at>
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


#define GETTEXT_PACKAGE "libparlatype"
#include <glib/gi18n-lib.h>

/* Initialize i18n for the library.

   Libraries have to be initialized just like applications, see
   GLib Reference Manual → GLib Utilities → Internationalization

   This is called from pt_player_new() and also from pt_waveloader_new()
   because PtWaveloader can be used without a PtPlayer. All other objects
   with translations are run after creating either a PtPlayer or PtWaveloader
   and don’t need this call anymore.

   “Inspired” by totem-pl-parser.c: totem_pl_parser_init_i18n().
*/


static gpointer
pt_i18n_real_init (gpointer data)
{
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	return NULL;
}

void
pt_i18n_init (void)
{
	static GOnce my_once = G_ONCE_INIT;
	g_once (&my_once, pt_i18n_real_init, NULL);
}
