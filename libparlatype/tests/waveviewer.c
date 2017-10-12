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
#include <src/pt-waveviewer.h>
#include <src/pt-player.h>


static void
waveviewer_empty (void)
{
	GtkWidget *testviewer;
	gboolean fixed_cursor, follow_cursor, has_selection, show_ruler;
	gint64 selection_end, selection_start;

	testviewer = pt_waveviewer_new ();
	g_assert (PT_IS_WAVEVIEWER (testviewer));
	g_object_get (testviewer,
		      "fixed-cursor", &fixed_cursor,
		      "follow-cursor", &follow_cursor,
		      "has-selection", &has_selection,
		      "show-ruler", &show_ruler,
		      "selection-end", &selection_end,
		      "selection-start", &selection_start,
		      NULL);
	g_assert_true (fixed_cursor);
	g_assert_true (follow_cursor);
	g_assert_false (has_selection);
	g_assert_true (show_ruler);
	g_assert_cmpint (selection_end, ==, 0);
	g_assert_cmpint (selection_start, ==, 0);

	pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (testviewer), FALSE);
	g_assert_false (pt_waveviewer_get_follow_cursor (PT_WAVEVIEWER (testviewer)));

	gtk_widget_destroy (testviewer);
}

static gboolean
stop_main (gpointer user_data)
{
	gtk_main_quit ();

	return G_SOURCE_REMOVE;
}


static void
waveviewer_loaded (void)
{
	GError    *error = NULL;
	PtPlayer  *player;
	PtWavedata *data;
	GtkWidget *window;
	GtkWidget *viewer;
	gchar     *testfile;
	gchar     *testuri;
	gboolean   success;

	player = pt_player_new (&error);
	g_assert_no_error (error);

	testfile = g_test_build_filename (G_TEST_DIST, "data/test1.ogg", NULL);
	testuri = g_strdup_printf ("file://%s", testfile);
	success = pt_player_open_uri (player, testuri, &error);

	g_assert_no_error (error);
	g_assert_true (success);

	data = pt_player_get_data (player, 100);
	viewer = pt_waveviewer_new ();
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 100);
	gtk_container_add (GTK_CONTAINER (window), viewer);
	gtk_widget_show_all (window);
	pt_waveviewer_set_wave (PT_WAVEVIEWER (viewer), data);
	pt_wavedata_free (data);

	g_timeout_add (200, stop_main, viewer);
	gtk_main ();

	g_free (testfile);
	g_free (testuri);
	g_object_unref (player);
	gtk_widget_destroy (window);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/waveviewer/empty", waveviewer_empty);
	g_test_add_func ("/waveviewer/loaded", waveviewer_loaded);
	gtk_init (NULL, NULL);

	return g_test_run ();
}
