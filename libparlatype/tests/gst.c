/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
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
#include <gst/gst.h>
#include <gst/audio/streamvolume.h>
#include "gst/gstptaudioasrbin.h"
#include "gst/gstptaudioplaybin.h"
#include "gst/gstptaudiobin.h"


static void
gst_audioplaybin_new (void)
{

	GstElement *audioplaybin;
	gdouble     volume;
	gboolean    mute;

	audioplaybin = gst_element_factory_make ("ptaudioplaybin", "testbin");
	g_assert_nonnull (audioplaybin);
	g_assert_true (GST_IS_STREAM_VOLUME (audioplaybin));

	g_object_get (audioplaybin, "volume", &volume, "mute", &mute, NULL);
	g_assert_cmpfloat (volume, ==, 1.0);
	g_assert_false (mute);

	gst_element_set_state (audioplaybin, GST_STATE_READY);
	g_object_set (audioplaybin, "volume", 0.5, "mute", TRUE, NULL);

	g_object_get (audioplaybin, "volume", &volume, "mute", &mute, NULL);
	g_assert_cmpfloat (volume, ==, 0.5);
	g_assert_true (mute);

	gst_element_set_state (audioplaybin, GST_STATE_NULL);
	gst_object_unref (audioplaybin);
}

static void
gst_audiobin_new (void)
{

	GstElement *audiobin;
	gdouble     volume;
	gboolean    mute;

	audiobin = gst_element_factory_make ("ptaudiobin", "testbin");
	g_assert_nonnull (audiobin);
	g_assert_true (GST_IS_STREAM_VOLUME (audiobin));

	g_object_get (audiobin, "volume", &volume, "mute", &mute, NULL);
	g_assert_cmpfloat (volume, ==, 1.0);
	g_assert_false (mute);

	gst_element_set_state (audiobin, GST_STATE_READY);
	g_object_set (audiobin, "volume", 0.5, "mute", TRUE, NULL);

	g_object_get (audiobin, "volume", &volume, "mute", &mute, NULL);
	g_assert_cmpfloat (volume, ==, 0.5);
	g_assert_true (mute);

	gst_element_set_state (audiobin, GST_STATE_NULL);
	gst_object_unref (audiobin);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	gst_init (NULL, NULL);
	gst_pt_audio_asr_bin_register ();
	gst_pt_audio_play_bin_register ();
	gst_pt_audio_bin_register ();

	g_test_add_func ("/gst/audioplaybin_new", gst_audioplaybin_new);
	g_test_add_func ("/gst/audiobin_new", gst_audiobin_new);

	return g_test_run ();
}
