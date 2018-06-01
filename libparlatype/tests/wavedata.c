/* Copyright (C) Gabor Karsay 2017 <gabor.karsay@gmx.at>
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
#include <pt-waveviewer.h>


static void
wavedata_new (void)
{
	PtWavedata *testdata1, *testdata2;
	float array[8] = {0.1, 0.1, 0.2, 0.2, 0.3, 0.3, 0.4, 0.4};
	
	testdata1 = pt_wavedata_new (array, 8, 1, 100);
	testdata2 = pt_wavedata_copy (testdata1);
	g_assert (testdata1 == testdata1);
	pt_wavedata_free (testdata1);
	pt_wavedata_free (testdata2);

	/* There are no tests, bogus input is possible */
	testdata1 = pt_wavedata_new (array, 123, 25, 34);
	pt_wavedata_free (testdata1);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/wavedata/new", wavedata_new);

	return g_test_run ();
}
