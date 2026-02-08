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

#include <pt-timestamp.h>

static void
timestamp_from_time (void)
{
  char *timestring;

  /* check arbitrary time strings, duration < 10 min, no rounding */
  timestring = pt_timestamp_from_time (1560, 10000, PT_PRECISION_SECOND);
  g_assert_cmpstr (timestring, ==, "0:01");
  g_free (timestring);

  timestring = pt_timestamp_from_time (1560, 10000, PT_PRECISION_SECOND_10TH);
  g_assert_cmpstr (timestring, ==, "0:01.5");
  g_free (timestring);

  timestring = pt_timestamp_from_time (1560, 10000, PT_PRECISION_SECOND_100TH);
  g_assert_cmpstr (timestring, ==, "0:01.56");
  g_free (timestring);

  /* duration >= 10 min */
  timestring = pt_timestamp_from_time (1560, 600000, PT_PRECISION_SECOND);
  g_assert_cmpstr (timestring, ==, "00:01");
  g_free (timestring);

  timestring = pt_timestamp_from_time (1560, 600000, PT_PRECISION_SECOND_10TH);
  g_assert_cmpstr (timestring, ==, "00:01.5");
  g_free (timestring);

  timestring = pt_timestamp_from_time (1560, 600000, PT_PRECISION_SECOND_100TH);
  g_assert_cmpstr (timestring, ==, "00:01.56");
  g_free (timestring);

  /* duration >= 1 hour */
  timestring = pt_timestamp_from_time (1560, 3600000, PT_PRECISION_SECOND);
  g_assert_cmpstr (timestring, ==, "0:00:01");
  g_free (timestring);

  timestring = pt_timestamp_from_time (1560, 3600000, PT_PRECISION_SECOND_10TH);
  g_assert_cmpstr (timestring, ==, "0:00:01.5");
  g_free (timestring);

  timestring = pt_timestamp_from_time (1560, 3600000, PT_PRECISION_SECOND_100TH);
  g_assert_cmpstr (timestring, ==, "0:00:01.56");
  g_free (timestring);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/timestamps/timestamp_from_time", timestamp_from_time);

  return g_test_run ();
}
