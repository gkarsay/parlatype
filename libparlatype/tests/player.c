/* Copyright (C) Gabor Karsay 2017-2018 <gabor.karsay@gmx.at>
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
#include <gst/gst.h> /* error domain */
#include <pt-player.h>


typedef struct {
	PtPlayer *testplayer;
	gchar    *testfile;
	gchar    *testuri;
} PtPlayerFixture;


static void
pt_player_fixture_set_up (PtPlayerFixture *fixture,
			  gconstpointer    user_data)
{
	GError *error = NULL;
	fixture->testplayer = NULL;
	gchar    *path;
	GFile    *file;
	gboolean  success;

	fixture->testplayer = pt_player_new ();
	pt_player_setup_player (fixture->testplayer, &error);
	g_assert_no_error (error);

	path = g_test_build_filename (G_TEST_DIST, "data", "test1.ogg", NULL);
	/* In make distcheck the path is something like /_build/../data/test1.ogg".
	   Although this works we need a "pretty" path to compare and assert it's the same.
	   To get rid of the "/_build/.." take a detour using GFile. */
	file = g_file_new_for_path (path);
	fixture->testfile = g_file_get_path (file);
	fixture->testuri = g_file_get_uri (file);

	success = pt_player_open_uri (fixture->testplayer, fixture->testuri, &error);

	g_assert_no_error (error);
	g_assert_true (success);

	g_object_unref (file);
	g_free (path);
}

static void
pt_player_fixture_tear_down (PtPlayerFixture *fixture,
			     gconstpointer    user_data)
{
	g_clear_object (&fixture->testplayer);
	g_free (fixture->testfile);
	g_free (fixture->testuri);
}

static void
player_new (void)
{
	/* create new PtPlayer and test initial properties */

	GError *error = NULL;
	PtPlayer *testplayer;
	gdouble speed, volume;

	testplayer = pt_player_new ();
	pt_player_setup_player (testplayer, &error);
	g_assert_no_error (error);
	g_assert (PT_IS_PLAYER (testplayer));
	g_object_get (testplayer,
		      "speed", &speed,
		      "volume", &volume,
		      NULL);
	g_assert_cmpfloat (speed, ==, 1.0);
	g_assert_cmpfloat (volume, ==, 1.0);
	g_object_unref (testplayer);
}

static void
player_open_fail (void)
{
	GError *error = NULL;
	PtPlayer *testplayer;
	gboolean  success;
	gchar    *path;
	GFile    *file;
	gchar    *uri;

	testplayer = pt_player_new ();
	pt_player_setup_player (testplayer, &error);
	g_assert_no_error (error);

	success = pt_player_open_uri (testplayer, "foo", &error);
	g_assert_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND);
	g_assert_false (success);
	g_clear_error (&error);

	path = g_test_build_filename (G_TEST_DIST, "data", "foo", NULL);
	file = g_file_new_for_path (path);
	uri = g_file_get_uri (file);
	success = pt_player_open_uri (testplayer, uri, &error);
	g_assert_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND);
	g_assert_false (success);
	g_clear_error (&error);
	g_object_unref (file);
	g_free (path);
	g_free (uri);

	path = g_test_build_filename (G_TEST_DIST, "data", "README", NULL);
	file = g_file_new_for_path (path);
	uri = g_file_get_uri (file);
	success = pt_player_open_uri (testplayer, uri, &error);
	g_assert_error (error, GST_STREAM_ERROR, GST_STREAM_ERROR_WRONG_TYPE);
	g_assert_false (success);
	g_clear_error (&error);
	g_object_unref (file);
	g_free (path);
	g_free (uri);

	g_object_unref (testplayer);
}

static void
player_open_ogg (PtPlayerFixture *fixture,
		 gconstpointer    user_data)
{
	/* open test file, get and check uri and filename */

	gchar *getchar;
	
	getchar = pt_player_get_uri (fixture->testplayer);
	g_assert_cmpstr (getchar, ==, fixture->testuri);
	g_free (getchar);

	getchar = pt_player_get_filename (fixture->testplayer);
	g_assert_cmpstr (getchar, ==, "test1.ogg");
	g_free (getchar);
}

typedef struct
{
	GAsyncResult *res;
	GMainLoop    *loop;
} SyncData;

static void
quit_loop_cb (PtPlayer	   *player,
	      GAsyncResult *res,
	      gpointer      user_data)
{
	SyncData *data = user_data;
	data->res = g_object_ref (res);
	g_main_loop_quit (data->loop);
}

static void
player_open_cancel (void)
{
	/* open test file async and cancel operation */

	GError *error = NULL;
	PtPlayer *testplayer;
	gchar    *testfile;
	gchar    *testuri;
	gboolean  success;
	SyncData      data;
	GMainContext *context;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	data.loop = g_main_loop_new (context, FALSE);
	data.res = NULL;

	testplayer = pt_player_new ();
	pt_player_setup_player (testplayer, &error);
	g_assert_no_error (error);

	testfile = g_test_build_filename (G_TEST_DIST, "data/test1.ogg", NULL);
	testuri = g_strdup_printf ("file://%s", testfile);
	pt_player_open_uri_async (testplayer, testuri, (GAsyncReadyCallback) quit_loop_cb, &data);

	pt_player_cancel (testplayer);
	g_main_loop_run (data.loop);

	success = pt_player_open_uri_finish (testplayer, data.res, &error);
	g_assert_false (success);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

	g_error_free (error);
	g_free (testfile);
	g_free (testuri);
	g_object_unref (testplayer);
	g_main_loop_unref (data.loop);
	g_object_unref (data.res);
}

static void
player_timestamps (PtPlayerFixture *fixture,
		   gconstpointer    user_data)
{
	gchar    *timestamp = NULL;
	gchar    *timestamp2;
	gboolean  valid;
	
	/* valid timestamps */
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "#0:01.2#", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "0:01.2", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "(0:01.2)", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "[0:01.2]", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "#0:01.21#", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "#0:01#", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "#00:00:01.2#", FALSE));

	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "#0:01-2#", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "0:01-2", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "(0:01-2)", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "[0:01-2]", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "#0:01-21#", FALSE));
	g_assert_true (pt_player_string_is_timestamp (fixture->testplayer, "#00:00:01-2#", FALSE));

	/* single/wrong delimiters */
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0:01.2", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "0:01.2#", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "(0:01.2", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "[0:01.2)", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "@0:01.2", FALSE));

	/* too many digits */
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0:001.2#", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0:01.200#", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#000:01.2#", FALSE));
	/* sec >= 60, min >= 60 */
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0:60.2#", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0:71.2#", FALSE));
	/* not enough digits */
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#:01.2#", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0:1.2#", FALSE));
	/* wrong delimiters */
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "(0.01.2)", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0.01.2#", FALSE));
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#0:01:2#", FALSE));
	/* too big, not valid in the test file */
	g_assert_false (pt_player_string_is_timestamp (fixture->testplayer, "#01:00.0#", TRUE));

	g_assert_cmpint (60200, ==, pt_player_get_timestamp_position (fixture->testplayer, "00:01:00-2", FALSE));

	pt_player_jump_to_position (fixture->testplayer, 1000);
	
	timestamp = pt_player_get_timestamp (fixture->testplayer);
	g_assert_nonnull (timestamp);

	valid = pt_player_string_is_timestamp (fixture->testplayer, timestamp, TRUE);
	g_assert_true (valid);

	pt_player_jump_to_position (fixture->testplayer, 0);
	timestamp2 = pt_player_get_timestamp (fixture->testplayer);
	g_assert_cmpstr (timestamp, !=, timestamp2);
	g_free (timestamp2);

	pt_player_goto_timestamp (fixture->testplayer, timestamp);
	timestamp2 = pt_player_get_timestamp (fixture->testplayer);
	g_assert_cmpstr (timestamp, ==, timestamp2);
		
	g_free (timestamp);
	g_free (timestamp2);
}

static void
player_volume_speed (PtPlayerFixture *fixture,
		     gconstpointer    user_data)
{
	/* set speed and volume properties */

	GError *error = NULL;
	PtPlayer *bind_player;
	gdouble speed, volume;
	
	pt_player_set_volume (fixture->testplayer, 0.7);
	pt_player_set_speed (fixture->testplayer, 0.7);

	g_object_get (fixture->testplayer,
		      "speed", &speed,
		      "volume", &volume,
		      NULL);

	g_assert_cmpfloat (volume, ==, 0.7);
	g_assert_cmpfloat (speed, ==, 0.7);

	/* mute doesn't change the volume */
	pt_player_mute_volume (fixture->testplayer, TRUE);
	g_object_get (fixture->testplayer, "volume", &volume, NULL);
	g_assert_cmpfloat (volume, ==, 0.7);

	pt_player_mute_volume (fixture->testplayer, FALSE);
	g_object_get (fixture->testplayer, "volume", &volume, NULL);
	g_assert_cmpfloat (volume, ==, 0.7);

	/* check bind property */
	bind_player = pt_player_new ();
	pt_player_setup_player (bind_player, &error);
	g_assert_no_error (error);

	g_object_bind_property (fixture->testplayer, "volume",
				bind_player, "volume",
				G_BINDING_DEFAULT);
	g_object_bind_property (fixture->testplayer, "speed",
				bind_player, "speed",
				G_BINDING_DEFAULT);

	pt_player_set_volume (fixture->testplayer, 0.5);
	pt_player_set_speed (fixture->testplayer, 0.5);

	g_object_get (bind_player,
		      "speed", &speed,
		      "volume", &volume,
		      NULL);
	g_assert_cmpfloat (volume, ==, 0.5);
	g_assert_cmpfloat (speed, ==, 0.5);

	g_object_unref (bind_player);	
}

static void
player_timestrings (PtPlayerFixture *fixture,
		    gconstpointer    user_data)
{
	gchar    *timestring;

	/* check time strings at current position */
	pt_player_jump_to_position (fixture->testplayer, 1000);

	timestring = pt_player_get_current_time_string (fixture->testplayer, PT_PRECISION_SECOND);
	g_assert_cmpstr (timestring, ==, "0:01");
	g_free (timestring);

	timestring = pt_player_get_current_time_string (fixture->testplayer, PT_PRECISION_SECOND_10TH);
	g_assert_cmpstr (timestring, ==, "0:01.0");
	g_free (timestring);

	timestring = pt_player_get_current_time_string (fixture->testplayer, PT_PRECISION_SECOND_100TH);
	g_assert_cmpstr (timestring, ==, "0:01.00");
	g_free (timestring);

	/* check duration time strings for given test file */
	timestring = pt_player_get_duration_time_string (fixture->testplayer, PT_PRECISION_SECOND);
	g_assert_cmpstr (timestring, ==, "0:10");
	g_free (timestring);

	timestring = pt_player_get_duration_time_string (fixture->testplayer, PT_PRECISION_SECOND_10TH);
	g_assert_cmpstr (timestring, ==, "0:10.0");
	g_free (timestring);

	timestring = pt_player_get_duration_time_string (fixture->testplayer, PT_PRECISION_SECOND_100TH);
	g_assert_cmpstr (timestring, ==, "0:10.06");
	g_free (timestring);

	/* check arbitrary time strings, duration < 10 min, no rounding */
	timestring = pt_player_get_time_string (1560, 10000, PT_PRECISION_SECOND);
	g_assert_cmpstr (timestring, ==, "0:01");
	g_free (timestring);

	timestring = pt_player_get_time_string (1560, 10000, PT_PRECISION_SECOND_10TH);
	g_assert_cmpstr (timestring, ==, "0:01.5");
	g_free (timestring);

	timestring = pt_player_get_time_string (1560, 10000, PT_PRECISION_SECOND_100TH);
	g_assert_cmpstr (timestring, ==, "0:01.56");
	g_free (timestring);

	/* duration >= 10 min */
	timestring = pt_player_get_time_string (1560, 600000, PT_PRECISION_SECOND);
	g_assert_cmpstr (timestring, ==, "00:01");
	g_free (timestring);

	timestring = pt_player_get_time_string (1560, 600000, PT_PRECISION_SECOND_10TH);
	g_assert_cmpstr (timestring, ==, "00:01.5");
	g_free (timestring);

	timestring = pt_player_get_time_string (1560, 600000, PT_PRECISION_SECOND_100TH);
	g_assert_cmpstr (timestring, ==, "00:01.56");
	g_free (timestring);

	/* duration >= 1 hour */
	timestring = pt_player_get_time_string (1560, 3600000, PT_PRECISION_SECOND);
	g_assert_cmpstr (timestring, ==, "0:00:01");
	g_free (timestring);

	timestring = pt_player_get_time_string (1560, 3600000, PT_PRECISION_SECOND_10TH);
	g_assert_cmpstr (timestring, ==, "0:00:01.5");
	g_free (timestring);

	timestring = pt_player_get_time_string (1560, 3600000, PT_PRECISION_SECOND_100TH);
	g_assert_cmpstr (timestring, ==, "0:00:01.56");
	g_free (timestring);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/player/new", player_new);
	g_test_add_func ("/player/open-fail", player_open_fail);
	g_test_add ("/player/open-ogg", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_open_ogg,
	            pt_player_fixture_tear_down);
	g_test_add_func ("/player/open-cancel", player_open_cancel);
	g_test_add ("/player/timestamps", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_timestamps,
	            pt_player_fixture_tear_down);
	g_test_add ("/player/volume_speed", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_volume_speed,
	            pt_player_fixture_tear_down);
	g_test_add ("/player/timestrings", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_timestrings,
	            pt_player_fixture_tear_down);

	return g_test_run ();
}
