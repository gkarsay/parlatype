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
#include <pt-player.h>


/* Helpers to turn async operations into sync ------------------------------- */

typedef struct
{
	GAsyncResult *res;
	GMainLoop    *loop;
} SyncData;

static SyncData
create_sync_data (void) {
	SyncData      data;
	GMainContext *context;

	/* Note: With other contexts than default the signal emitting test
	 * doesn't work. PtWaveloader uses g_timeout_add() and that doesn't
	 * work with g_main_context_push_thread_default (context). */
	context = g_main_context_default ();

	data.loop = g_main_loop_new (context, FALSE);
	data.res = NULL;
	return data;
}

static void
free_sync_data (SyncData data) {
	g_main_loop_unref (data.loop);
	g_object_unref (data.res);
}

static void
quit_loop_cb (PtWaveviewer *wl,
	      GAsyncResult *res,
	      gpointer      user_data)
{
	SyncData *data = user_data;
	data->res = g_object_ref (res);
	g_main_loop_quit (data->loop);
}


/* Tests -------------------------------------------------------------------- */

static void
waveviewer_empty (void)
{
	GtkWidget *testviewer;
	gboolean fixed_cursor, follow_cursor, has_selection, show_ruler;
	gint64 selection_end, selection_start;

	testviewer = pt_waveviewer_new ();
	g_assert_true (PT_IS_WAVEVIEWER (testviewer));
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

static void
waveviewer_loaded (void)
{
	GError    *error = NULL;
	PtPlayer  *player;
	GtkWidget *window;
	GtkWidget *viewer;
	GFile     *testfile;
	gchar     *testpath;
	gchar     *testuri;
	gboolean   success;
	SyncData   data;

	player = pt_player_new ();
	pt_player_setup_player (player, TRUE);

	testpath = g_test_build_filename (G_TEST_DIST, "data", "tick-10sec.ogg", NULL);
	testfile = g_file_new_for_path (testpath);
	testuri = g_file_get_uri (testfile);
	success = pt_player_open_uri (player, testuri);
	g_assert_true (success);

	data = create_sync_data ();
	viewer = pt_waveviewer_new ();
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 100);
	gtk_container_add (GTK_CONTAINER (window), viewer);
	gtk_widget_show_all (window);
	pt_waveviewer_load_wave_async (PT_WAVEVIEWER (viewer), testuri, NULL,
				       (GAsyncReadyCallback) quit_loop_cb, &data);

	g_main_loop_run (data.loop);
	success = pt_waveviewer_load_wave_finish (PT_WAVEVIEWER (viewer), data.res, &error);
	g_assert_true (success);
	g_assert_no_error (error);

	g_free (testpath);
	g_free (testuri);
	g_object_unref (testfile);
	free_sync_data (data);
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
