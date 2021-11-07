/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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


#include <glib.h>
#include "contrib/gnome-languages.h"


static void
gnome_languages (void)
{
	/* Existing ISO 639 language code */
	gchar *result = NULL;
	result = gnome_get_language_from_locale ("de", NULL);
	g_assert_nonnull (result);
	g_assert_cmpstr (result, ==, "German");
	g_free (result);

	/* Existing ISO 639 language code + existing ISO 3166 territory code */
	result = NULL;
	result = gnome_get_language_from_locale ("de_AT", NULL);
	g_assert_nonnull (result);
	g_assert_cmpstr (result, ==, "German (Austria)");
	g_free (result);

	/* Existing ISO 639-3 language code */
	result = NULL;
	result = gnome_get_language_from_locale ("gsw", NULL);
	g_assert_nonnull (result);
	g_assert_cmpstr (result, ==, "German, Swiss");
	g_free (result);

	/* Existing ISO 639-3 language code + existing ISO 3166 territory code */
	result = NULL;
	result = gnome_get_language_from_locale ("gsw_AT", NULL);
	g_assert_nonnull (result);
	g_assert_cmpstr (result, ==, "German, Swiss (Austria)");
	g_free (result);

	/* Existing ISO 639 language code + wrong ISO 3166 territory code */
	result = NULL;
	result = gnome_get_language_from_locale ("de_XX", NULL);
	g_assert_nonnull (result);
	g_assert_cmpstr (result, ==, "German");
	g_free (result);

	/* Existing ISO 639-3 language code + wrong ISO 3166 territory code */
	result = NULL;
	result = gnome_get_language_from_locale ("gsw_XX", NULL);
	g_assert_nonnull (result);
	g_assert_cmpstr (result, ==, "German, Swiss");
	g_free (result);

	/* Wrong ISO 639 language code */
	result = NULL;
	result = gnome_get_language_from_locale ("xx", NULL);
	g_assert_null (result);

	/* Wrong ISO 639-3 language code */
	result = gnome_get_language_from_locale ("xxx", NULL);
	g_assert_null (result);

	/* Wrong ISO 639 language code + existing ISO 3166 territory code */
	result = gnome_get_language_from_locale ("xx_AT", NULL);
	g_assert_null (result);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/contrib/gnome-languages", gnome_languages);

	return g_test_run ();
}
