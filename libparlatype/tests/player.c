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
#include "mock-plugin.h"


/* Fixture ------------------------------------------------------------------ */

typedef struct {
	PtPlayer *testplayer;
	gchar    *testfile;
	gchar    *testuri;
} PtPlayerFixture;

static void
pt_player_fixture_set_up (PtPlayerFixture *fixture,
			  gconstpointer    user_data)
{
	fixture->testplayer = NULL;
	gchar    *path;
	GFile    *file;
	gboolean  success;

	fixture->testplayer = pt_player_new ();
	pt_player_setup_player (fixture->testplayer, TRUE);

	path = g_test_build_filename (G_TEST_DIST, "data", "tick-10sec.ogg", NULL);
	/* In make distcheck the path is something like /_build/../data/test1.ogg".
	   Although this works we need a "pretty" path to compare and assert it's the same.
	   To get rid of the "/_build/.." take a detour using GFile. */
	file = g_file_new_for_path (path);
	fixture->testfile = g_file_get_path (file);
	fixture->testuri = g_file_get_uri (file);

	success = pt_player_open_uri (fixture->testplayer, fixture->testuri);
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


/* Tests -------------------------------------------------------------------- */

static void
player_new (void)
{
	/* create new PtPlayer and test initial properties */

	PtPlayer *testplayer;

	/* Construction properties */
	gdouble speed, volume;
	gboolean t_fixed;
	gint precision;
	gchar *t_delimiter, *t_separator;

	/* Read/write properties TODO why not construction? */
	gboolean repeat_all, repeat_selection;
	gint pause, back, forward;

	testplayer = pt_player_new ();
	pt_player_setup_player (testplayer, TRUE);
	g_assert_true (PT_IS_PLAYER (testplayer));
	g_object_set (testplayer,
		      "pause", 0,
		      "back", 10000,
		      "forward", 10000,
		      "repeat-all", FALSE,
		      "repeat-selection", FALSE,
		      NULL);
	g_object_get (testplayer,
		      "speed", &speed,
		      "volume", &volume,
		      "timestamp-precision", &precision,
		      "timestamp-fixed", &t_fixed,
		      "timestamp-delimiter", &t_delimiter,
		      "timestamp-fraction-sep", &t_separator,
		      "pause", &pause,
		      "back", &back,
		      "forward", &forward,
		      "repeat-all", &repeat_all,
		      "repeat-selection", &repeat_selection,
		      NULL);
	g_assert_cmpfloat (speed, ==, 1.0);
	g_assert_cmpfloat (volume, ==, 1.0);
	g_assert_false (t_fixed);
	g_assert_false (repeat_all);
	g_assert_false (repeat_selection);
	g_assert_cmpint (precision, ==, PT_PRECISION_SECOND_10TH);
	g_assert_cmpint (pause, ==, 0);
	g_assert_cmpint (back, ==, 10000);
	g_assert_cmpint (forward, ==, 10000);
	g_assert_cmpstr (t_delimiter, ==, "#");
	g_assert_cmpstr (t_separator, ==, ".");

	g_assert_cmpint (pt_player_get_pause (testplayer), ==, pause);
	g_assert_cmpint (pt_player_get_back (testplayer), ==, back);
	g_assert_cmpint (pt_player_get_forward (testplayer), ==, forward);

	g_free (t_delimiter);
	g_free (t_separator);
	g_object_unref (testplayer);
}

typedef struct
{
	GQuark     domain;
	gint       code;
	GMainLoop *loop;
} ErrorCBData;

static void
error_cb (PtPlayer *player,
	  GError   *error,
	  gpointer  user_data)
{
	ErrorCBData *data = user_data;
	g_assert_error (error, data->domain, data->code);
	g_main_loop_quit (data->loop);
}

static void
player_open_fail (void)
{
	PtPlayer    *player;
	ErrorCBData  data;
	gboolean     success;
	gchar       *path;
	GFile       *file;
	gchar       *uri;

	player = pt_player_new ();
	pt_player_setup_player (player, TRUE);

	/* Fails to open "foo" */
	data.loop = g_main_loop_new (g_main_context_default (), FALSE);
	data.domain = GST_RESOURCE_ERROR;
	data.code = GST_RESOURCE_ERROR_NOT_FOUND;
	g_signal_connect (player, "error", G_CALLBACK (error_cb), &data);
	success = pt_player_open_uri (player, "foo");
	g_main_loop_run (data.loop);
	g_assert_false (success);

	/* Fails to open a valid path, not existing file.
	 * Error domain and code unchanged. */
	path = g_test_build_filename (G_TEST_DIST, "data", "foo", NULL);
	file = g_file_new_for_path (path);
	uri = g_file_get_uri (file);
	success = pt_player_open_uri (player, uri);
	g_main_loop_run (data.loop);
	g_assert_false (success);

	g_object_unref (file);
	g_free (path);
	g_free (uri);

	/* Fails to open an existing file of wrong type */
	data.domain = GST_STREAM_ERROR;
	data.code = GST_STREAM_ERROR_WRONG_TYPE;
	path = g_test_build_filename (G_TEST_DIST, "data", "README", NULL);
	file = g_file_new_for_path (path);
	uri = g_file_get_uri (file);
	success = pt_player_open_uri (player, uri);
	g_main_loop_run (data.loop);
	g_assert_false (success);

	g_object_unref (file);
	g_free (path);
	g_free (uri);
	g_main_loop_unref (data.loop);
	g_object_unref (player);
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
	g_assert_cmpstr (getchar, ==, "tick-10sec.ogg");
	g_free (getchar);
}

static void
player_selections (PtPlayerFixture *fixture,
                   gconstpointer    user_data)
{
	/* set, get and clear a selection */

	gboolean selection;

	selection = pt_player_selection_active (fixture->testplayer);
	g_assert_false (selection);

	pt_player_set_selection (fixture->testplayer, 1000, 2000);
	selection = pt_player_selection_active (fixture->testplayer);
	g_assert_true (selection);

	pt_player_clear_selection (fixture->testplayer);
	selection = pt_player_selection_active (fixture->testplayer);
	g_assert_false (selection);
}

static void
player_timestamps (PtPlayerFixture *fixture,
		   gconstpointer    user_data)
{
	gchar    *timestamp = NULL;
	gchar    *timestamp2;
	gboolean  valid;

	/* test for delimiters */
	g_object_set (fixture->testplayer, "timestamp-delimiter", "#", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.0#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-delimiter", "(", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "(0:00.0)");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-delimiter", "[", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "[0:00.0]");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-delimiter", "None", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "0:00.0");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-delimiter", "something-else", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.0#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-delimiter", NULL, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.0#");
	g_free (timestamp);

	/* test for fraction separator */
	g_object_set (fixture->testplayer, "timestamp-fraction-sep", ".", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.0#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-fraction-sep", "-", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00-0#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-fraction-sep", "something-else", NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.0#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-fraction-sep", NULL, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.0#");
	g_free (timestamp);

	/* Test all code paths of pt_player_get_timestamp_for_time */
	g_object_set (fixture->testplayer, "timestamp-fixed", TRUE, "timestamp-fraction-sep", ".", NULL);
	g_object_set (fixture->testplayer, "timestamp-precision", PT_PRECISION_SECOND, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#00:00:00#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-precision", PT_PRECISION_SECOND_10TH, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#00:00:00.0#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-precision", PT_PRECISION_SECOND_100TH, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#00:00:00.00#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-fixed", FALSE, NULL);
	g_object_set (fixture->testplayer, "timestamp-precision", PT_PRECISION_SECOND, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00#");
	g_free (timestamp);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 3600000);
	g_assert_cmpstr (timestamp, ==, "#0:00:00#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-precision", PT_PRECISION_SECOND_10TH, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.0#");
	g_free (timestamp);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 3600000);
	g_assert_cmpstr (timestamp, ==, "#0:00:00.0#");
	g_free (timestamp);
	g_object_set (fixture->testplayer, "timestamp-precision", PT_PRECISION_SECOND_100TH, NULL);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 1);
	g_assert_cmpstr (timestamp, ==, "#0:00.00#");
	g_free (timestamp);
	timestamp = pt_player_get_timestamp_for_time (fixture->testplayer, 0, 3600000);
	g_assert_cmpstr (timestamp, ==, "#0:00:00.00#");
	g_free (timestamp);

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

	PtPlayer *bind_player;
	gdouble speed, volume;

	pt_player_set_volume (fixture->testplayer, 0.7);
	pt_player_set_speed (fixture->testplayer, 0.7);

	g_object_get (fixture->testplayer,
		      "speed", &speed,
		      "volume", &volume,
		      NULL);

	g_assert_cmpfloat (speed, ==, 0.7);
	/* some audiosinks (pulsesink, directsoundsink) need a tolerance */
	g_assert_cmpfloat_with_epsilon (volume, 0.7, 0.00001);

	/* mute doesn't change the volume */
	pt_player_set_mute (fixture->testplayer, TRUE);
	g_object_get (fixture->testplayer, "volume", &volume, NULL);
	g_assert_cmpfloat_with_epsilon (volume, 0.7, 0.00001);
	g_assert_true (pt_player_get_mute (fixture->testplayer));

	pt_player_set_mute (fixture->testplayer, FALSE);
	g_object_get (fixture->testplayer, "volume", &volume, NULL);
	g_assert_cmpfloat_with_epsilon (volume, 0.7, 0.00001);
	g_assert_false (pt_player_get_mute (fixture->testplayer));

	/* check bind property */
	bind_player = pt_player_new ();
	pt_player_setup_player (bind_player, TRUE);

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
	g_assert_cmpstr (timestring, ==, "0:09");
	g_free (timestring);

	timestring = pt_player_get_duration_time_string (fixture->testplayer, PT_PRECISION_SECOND_10TH);
	g_assert_cmpstr (timestring, ==, "0:09.9");
	g_free (timestring);

	timestring = pt_player_get_duration_time_string (fixture->testplayer, PT_PRECISION_SECOND_100TH);
	g_assert_cmpstr (timestring, ==, "0:09.98");
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

static void
player_config_loadable (void)
{
	PtPlayer *testplayer;
	PtConfig *good, *bad;
	GFile    *testfile;
	gchar    *testpath;
	gboolean  success;

	/* Create player */
	testplayer = pt_player_new ();
	pt_player_setup_player (testplayer, TRUE);
	g_assert_true (PT_IS_PLAYER (testplayer));
	mock_plugin_register ();

	/* Create config with existing plugin */
	testpath = g_test_build_filename (G_TEST_DIST, "data", "config-mock-plugin.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	good = pt_config_new (testfile);
	g_assert_true (pt_config_is_valid (good));
	g_object_unref (testfile);
	g_free (testpath);

	/* Create config with missing plugin*/
	testpath = g_test_build_filename (G_TEST_DIST, "data", "config-test.asr", NULL);
	testfile = g_file_new_for_path (testpath);
	bad = pt_config_new (testfile);
	g_assert_true (pt_config_is_valid (bad));
	g_object_unref (testfile);
	g_free (testpath);

	success = pt_player_config_is_loadable (testplayer, good);
	g_assert_true (success);

	success = pt_player_config_is_loadable (testplayer, bad);
	g_assert_false (success);

	g_object_unref (good);
	g_object_unref (bad);
	g_object_unref (testplayer);
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
	g_test_add ("/player/selections", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_selections,
	            pt_player_fixture_tear_down);
	g_test_add ("/player/timestamps", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_timestamps,
	            pt_player_fixture_tear_down);
	g_test_add ("/player/volume_speed", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_volume_speed,
	            pt_player_fixture_tear_down);
	g_test_add ("/player/timestrings", PtPlayerFixture, NULL,
	            pt_player_fixture_set_up, player_timestrings,
	            pt_player_fixture_tear_down);
	g_test_add_func ("/player/config-loadable", player_config_loadable);

	return g_test_run ();
}
