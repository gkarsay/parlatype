/* Copyright 2026 Gabor Karsay <gabor.karsay@gmx.at>
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

#include "pt-utils.h"

#include <glib.h>

static void
pretty_download_size (void)
{
  char *string;

  string = pt_utils_pretty_download_size (0);
  g_assert_cmpstr (string, ==, "Unknown");
  g_free (string);

  string = pt_utils_pretty_download_size (1);
  g_assert_cmpstr (string, ==, "1 KB");
  g_free (string);

  string = pt_utils_pretty_download_size (1001);
  g_assert_cmpstr (string, ==, "1 KB");
  g_free (string);

  string = pt_utils_pretty_download_size (1999);
  g_assert_cmpstr (string, ==, "1 KB");
  g_free (string);

  string = pt_utils_pretty_download_size (1100100);
  g_assert_cmpstr (string, ==, "1 MB");
  g_free (string);

  string = pt_utils_pretty_download_size (1100100100);
  g_assert_cmpstr (string, ==, "1.1 GB");
  g_free (string);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/utils/pretty_download_size", pretty_download_size);

  return g_test_run ();
}
