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
#include <src/pt-waveloader.h>


static void
waveloader_new (void)
{
	/* test creation and property, everything else is tested
	   indirectly via PtPlayer */

	PtWaveloader *testloader;
	gchar *uri;

	testloader = pt_waveloader_new (NULL);
	g_assert (PT_IS_WAVELOADER (testloader));
	g_object_get (testloader, "uri", &uri, NULL);
	g_assert_null (uri);
	g_object_unref (testloader);

	testloader = pt_waveloader_new ("file");
	g_assert (PT_IS_WAVELOADER (testloader));
	g_object_get (testloader, "uri", &uri, NULL);
	g_assert_cmpstr (uri, ==, "file");
	g_free (uri);

	g_object_set (testloader, "uri", "some-other-file", NULL);
	g_object_get (testloader, "uri", &uri, NULL);
	g_assert_cmpstr (uri, ==, "some-other-file");
	g_free (uri);

	/* Doesn't raise code coverage 
	g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_WARNING, "*has no property named*");
	g_object_get (testloader, "not-a-property", &uri, NULL);
	g_test_assert_expected_messages ();

	g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_WARNING, "*has no property named*");
	g_object_set (testloader, "not-a-property", "fail", NULL);
	g_test_assert_expected_messages (); */

	g_object_unref (testloader);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/waveloader/new", waveloader_new);

	return g_test_run ();
}
