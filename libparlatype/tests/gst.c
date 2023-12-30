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

#include "config.h"

#include "gst/gstptaudioasrbin.h"
#include "gst/gstptaudiobin.h"
#include "gst/gstptaudioplaybin.h"
#include "mock-plugin.h"

#include <glib.h>
#include <gst/audio/streamvolume.h>
#include <gst/gst.h>

/* Helpers to turn async operations into sync ------------------------------- */

typedef struct
{
  GAsyncResult *res;
  GMainLoop    *loop;
} SyncData;

static SyncData
create_sync_data (void)
{
  SyncData      data;
  GMainContext *context;

  context = g_main_context_default ();
  data.loop = g_main_loop_new (context, FALSE);
  data.res = NULL;

  return data;
}

static void
free_sync_data (SyncData data)
{
  g_main_loop_unref (data.loop);
  g_object_unref (data.res);
}

static void
quit_loop_cb (GstPtAudioAsrBin *bin,
              GAsyncResult     *res,
              gpointer          user_data)
{
  SyncData *data = user_data;
  data->res = g_object_ref (res);
  g_main_loop_quit (data->loop);
}

/* Tests -------------------------------------------------------------------- */

static void
gst_audioasrbin (void)
{

  GstPtAudioAsrBin *asr;
  PtConfig         *good, *bad;
  GFile            *testfile;
  gchar            *testpath;
  GError           *error = NULL;
  gboolean          success;
  SyncData          data;

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

  /* Create Test-Element */
  asr = GST_PT_AUDIO_ASR_BIN (gst_element_factory_make ("ptaudioasrbin", "testbin"));
  g_assert_nonnull (asr);
  g_assert_false (gst_pt_audio_asr_bin_is_configured (asr));

  /* Apply existing plugin */
  data = create_sync_data ();
  gst_pt_audio_asr_bin_configure_asr_async (asr, good, NULL, (GAsyncReadyCallback) quit_loop_cb, &data);
  g_main_loop_run (data.loop);
  success = gst_pt_audio_asr_bin_configure_asr_finish (asr, data.res, &error);
  g_assert_true (success);
  g_assert_no_error (error);
  g_assert_true (gst_pt_audio_asr_bin_is_configured (asr));
  free_sync_data (data);

  /* Apply missing plugin */
  data = create_sync_data ();
  gst_pt_audio_asr_bin_configure_asr_async (asr, bad, NULL, (GAsyncReadyCallback) quit_loop_cb, &data);
  g_main_loop_run (data.loop);
  success = gst_pt_audio_asr_bin_configure_asr_finish (asr, data.res, &error);
  g_assert_false (success);
  g_assert_error (error, GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN);
  g_assert_false (gst_pt_audio_asr_bin_is_configured (asr));
  g_clear_error (&error);
  free_sync_data (data);

  g_object_unref (good);
  g_object_unref (bad);
  gst_object_unref (asr);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gst_init (NULL, NULL);
  gst_pt_audio_asr_bin_register ();
  gst_pt_audio_play_bin_register ();
  gst_pt_audio_bin_register ();
  mock_plugin_register ();

  g_test_add_func ("/gst/audioasrbin_new", gst_audioasrbin);

  return g_test_run ();
}
