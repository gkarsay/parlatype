/* Copyright (C) Gabor Karsay 2022 <gabor.karsay@gmx.at>
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


#include <pt-waveloader.c>



/* Tests -------------------------------------------------------------------- */

static void
test_convert_one_second (void)
{
	/* Resize a couple of input sizes to all possible zoom levels (pps from 25 to 200).
	 * Values are zero, we are testing output size, not content. */


	GArray *in = g_array_new (FALSE,	/* zero terminated */
	                          TRUE,		/* clear to zero   */
	                          sizeof(gint16));

	GArray *out = g_array_new (FALSE, TRUE, sizeof(float));

	int index_in;
	int index_out;
	int out_size;
	int pps;

	int testsize[4] = {7000,  /* less than a second  */
	                   8000,  /* exactly a second    */
	                   8111,  /* a prime > 1 second  */
	                  17029   /* a prime > 2 seconds */
	};


	for (int i = 0; i < 4; i++) {
		g_array_set_size (in, testsize[i]);

		for (pps = 25; pps <= 200; pps++) {
			index_in = 0;
			index_out = 0;
			out_size = calc_lowres_len (in->len, pps);
			g_array_set_size (out, out_size);

			while (in->len > index_in) {
				convert_one_second (in, out, &index_in, &index_out, pps);
			}

			g_assert_cmpint (index_in, ==, testsize[i]);
			g_assert_cmpint (index_out, ==, out_size);
		}
	}

	g_array_unref (in);
	g_array_unref (out);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/waveloader-static/convert", test_convert_one_second);

	return g_test_run ();
}
