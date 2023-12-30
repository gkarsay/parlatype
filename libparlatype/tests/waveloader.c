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
#include <gst/gst.h> /* error domain */
#include <pt-waveloader.h>

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
quit_loop_cb (PtWaveloader *wl,
              GAsyncResult *res,
              gpointer      user_data)
{
  SyncData *data = user_data;
  data->res = g_object_ref (res);
  g_main_loop_quit (data->loop);
}

/* Other helpers------------------------------------------------------------- */

static PtWaveloader *
wl_with_test_uri (const gchar *name)
{
  PtWaveloader *wl;
  gchar        *path;
  gchar        *uri;
  GFile        *file;

  path = g_test_build_filename (G_TEST_DIST, "data", name, NULL);
  file = g_file_new_for_path (path);
  uri = g_file_get_uri (file);
  wl = pt_waveloader_new (uri);

  g_free (uri);
  g_free (path);
  g_object_unref (file);

  return wl;
}

/* Tests -------------------------------------------------------------------- */

static void
waveloader_new (void)
{
  /* Test construction and setting/getting properties */

  PtWaveloader *wl;
  gchar        *uri;

  wl = pt_waveloader_new (NULL);
  g_assert_true (PT_IS_WAVELOADER (wl));
  g_object_get (wl, "uri", &uri, NULL);
  g_assert_null (uri);
  g_object_unref (wl);

  wl = pt_waveloader_new ("file");
  g_assert_true (PT_IS_WAVELOADER (wl));
  g_object_get (wl, "uri", &uri, NULL);
  g_assert_cmpstr (uri, ==, "file");
  g_free (uri);

  g_object_set (wl, "uri", "some-other-file", NULL);
  g_object_get (wl, "uri", &uri, NULL);
  g_assert_cmpstr (uri, ==, "some-other-file");
  g_free (uri);

  g_object_unref (wl);
}

static void
waveloader_load_fail (void)
{
  /* Test loading not existent file: should fail and return error */

  PtWaveloader *wl;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;

  data = create_sync_data ();
  wl = pt_waveloader_new ("file:///some-file-that-does-not-exist");
  pt_waveloader_load_async (wl, 100,
                            NULL,
                            (GAsyncReadyCallback) quit_loop_cb,
                            &data);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_false (success);
  g_assert_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND);

  g_clear_error (&error);
  free_sync_data (data);
  g_object_unref (wl);
}

static void
waveloader_load_cancel (void)
{
  /* Test cancelling load operation: should cancel and return error */

  PtWaveloader *wl;
  GCancellable *cancel;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;

  data = create_sync_data ();
  wl = wl_with_test_uri ("tick-10sec.ogg");
  cancel = g_cancellable_new ();
  pt_waveloader_load_async (wl, 100,
                            cancel,
                            (GAsyncReadyCallback) quit_loop_cb,
                            &data);
  g_cancellable_cancel (cancel);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_false (success);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  g_clear_error (&error);
  free_sync_data (data);
  g_object_unref (cancel);
  g_object_unref (wl);
}

static void
waveloader_resize_cancel (void)
{
  /* Test cancelling resize operation: should fail and return error */

  PtWaveloader *wl;
  GCancellable *cancel;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;

  data = create_sync_data ();
  wl = wl_with_test_uri ("tick-10sec.ogg");
  cancel = g_cancellable_new ();
  pt_waveloader_load_async (wl, 100,
                            NULL,
                            (GAsyncReadyCallback) quit_loop_cb,
                            &data);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_true (success);
  g_assert_no_error (error);
  g_object_unref (data.res);

  pt_waveloader_resize_async (wl, 100, cancel,
                              (GAsyncReadyCallback) quit_loop_cb,
                              &data);
  g_cancellable_cancel (cancel);
  g_main_loop_run (data.loop);

  success = pt_waveloader_resize_finish (wl, data.res, &error);
  g_assert_false (success);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  g_clear_error (&error);
  free_sync_data (data);
  g_object_unref (cancel);
  g_object_unref (wl);
}

static void
waveloader_resize_without_load (void)
{
  /* Test resize operation without loading before: should fail and return error */

  PtWaveloader *wl;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;

  data = create_sync_data ();
  wl = wl_with_test_uri ("tick-10sec.ogg");
  pt_waveloader_resize_async (wl, 100, NULL,
                              (GAsyncReadyCallback) quit_loop_cb,
                              &data);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_false (success);
  g_assert_error (error, GST_CORE_ERROR, GST_CORE_ERROR_FAILED);

  g_clear_error (&error);
  free_sync_data (data);
  g_object_unref (wl);
}

static void
waveloader_load_pending (void)
{
  /* Test load operation without waiting for or cancelling previous load
   * operation: should fail and return error */

  PtWaveloader *wl;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;

  data = create_sync_data ();
  wl = wl_with_test_uri ("tick-10sec.ogg");
  pt_waveloader_load_async (wl, 100,
                            NULL,
                            NULL,
                            NULL);
  pt_waveloader_load_async (wl, 100,
                            NULL,
                            (GAsyncReadyCallback) quit_loop_cb,
                            &data);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_false (success);
  g_assert_error (error, GST_CORE_ERROR, GST_CORE_ERROR_FAILED);

  g_clear_error (&error);
  free_sync_data (data);
  g_object_unref (wl);
}

static void
waveloader_load_unref (void)
{
  /* Test unreffing waveloader while it is loading. Should dispose
   * gracefully. */

  PtWaveloader *wl;

  wl = wl_with_test_uri ("tick-10sec.ogg");
  pt_waveloader_load_async (wl, 100, NULL, NULL, NULL);
  g_object_unref (wl);
}

static gint    array_emit_count = 0;
static gint    progress_emit_count = 0;
static gdouble first_progress = 0;
static gdouble last_progress = 0;

static void
progress_cb (PtWaveloader *wl,
             gdouble       progress)
{
  progress_emit_count++;
  if (progress_emit_count == 1)
    first_progress = progress;
  last_progress = progress;
}

static void
array_size_changed_cb (PtWaveloader *wl)
{
  array_emit_count++;
}

static void
waveloader_load_success (void)
{
  /* Test a successful load: should return success, no error.
   * Should emit several progress signals, starting from > 0 and ending
   * at 1.0. Should emit at least one array-size-changed signal.
   * Results for the loaded file should be as in previous runs: check
   * array length and an arbitrary index value. */

  PtWaveloader *wl;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;
  GArray       *array;

  data = create_sync_data ();
  wl = wl_with_test_uri ("tick-60sec.ogg");
  array = pt_waveloader_get_data (wl);
  g_signal_connect (wl, "progress",
                    G_CALLBACK (progress_cb), NULL);
  g_signal_connect (wl, "array-size-changed",
                    G_CALLBACK (array_size_changed_cb), NULL);
  pt_waveloader_load_async (wl, 100,
                            NULL,
                            (GAsyncReadyCallback) quit_loop_cb,
                            &data);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_true (success);
  g_assert_no_error (error);

  /* Check signal emission. */
  g_assert_cmpint (progress_emit_count, >=, 2);
  g_assert_cmpfloat (first_progress, >, 0);
  g_assert_cmpfloat (first_progress, <, 1);
  g_assert_cmpfloat (last_progress, ==, 1);

  g_assert_cmpint (array_emit_count, >=, 1);
  g_assert_cmpint (array->len, ==, 11982);

  /* float value varies a bit! */
  g_assert_cmpfloat_with_epsilon (g_array_index (array, float, 0), -0.796143, 0.001);
  g_assert_cmpfloat_with_epsilon (g_array_index (array, float, 42), 0.0, 0.001);

  g_object_unref (data.res);

  /* Resize operation: should return success, no error. Should emit
   * a array-size-changed signal. The new array size should be different. */

  pt_waveloader_resize_async (wl, 99, NULL,
                              (GAsyncReadyCallback) quit_loop_cb,
                              &data);
  g_main_loop_run (data.loop);
  success = pt_waveloader_resize_finish (wl, data.res, &error);
  g_assert_no_error (error);
  g_assert_true (success);
  g_assert_cmpint (array_emit_count, >=, 2);
  g_assert_cmpint (array->len, <, 11982);

  free_sync_data (data);
  g_object_unref (wl);
}

static void
waveloader_resize_sync (void)
{
  /* Test the sync version of resize: should succeed, no error */

  PtWaveloader *wl;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;

  data = create_sync_data ();
  wl = wl_with_test_uri ("tick-10sec.ogg");
  pt_waveloader_load_async (wl, 100,
                            NULL,
                            (GAsyncReadyCallback) quit_loop_cb,
                            &data);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_true (success);
  g_assert_no_error (error);

  success = pt_waveloader_resize (wl, 200, &error);
  g_assert_true (success);
  g_assert_no_error (error);

  free_sync_data (data);
  g_object_unref (wl);
}

static void
waveloader_compare_load_resize (void)
{
  /* Test that loading gives the same data as resizing. Resize and then
   * resize back: array length should be the same, an arbitrary index
   * value should be the same. */

  PtWaveloader *wl;
  SyncData      data;
  GError       *error = NULL;
  gboolean      success;
  GArray       *array;
  gint          len;
  float         value;

  data = create_sync_data ();
  wl = wl_with_test_uri ("tick-10sec.ogg");
  array = pt_waveloader_get_data (wl);
  pt_waveloader_load_async (wl, 100,
                            NULL,
                            (GAsyncReadyCallback) quit_loop_cb,
                            &data);
  g_main_loop_run (data.loop);

  success = pt_waveloader_load_finish (wl, data.res, &error);
  g_assert_true (success);
  g_assert_no_error (error);

  len = array->len;
  value = g_array_index (array, float, 42);

  g_object_unref (data.res);
  pt_waveloader_resize_async (wl, 150, NULL,
                              (GAsyncReadyCallback) quit_loop_cb,
                              &data);
  g_main_loop_run (data.loop);
  success = pt_waveloader_resize_finish (wl, data.res, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  g_object_unref (data.res);
  pt_waveloader_resize_async (wl, 100, NULL,
                              (GAsyncReadyCallback) quit_loop_cb,
                              &data);
  g_main_loop_run (data.loop);
  success = pt_waveloader_resize_finish (wl, data.res, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  g_assert_cmpint (len, ==, array->len);
  g_assert_cmpfloat (value, ==, g_array_index (array, float, 42));

  free_sync_data (data);
  g_object_unref (wl);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/waveloader/new", waveloader_new);
  g_test_add_func ("/waveloader/load_fail", waveloader_load_fail);
  g_test_add_func ("/waveloader/load_cancel", waveloader_load_cancel);
  g_test_add_func ("/waveloader/load_unref", waveloader_load_unref);
  g_test_add_func ("/waveloader/resize_cancel", waveloader_resize_cancel);
  g_test_add_func ("/waveloader/resize_without_load", waveloader_resize_without_load);
  g_test_add_func ("/waveloader/load_pending", waveloader_load_pending);
  g_test_add_func ("/waveloader/load_success", waveloader_load_success);
  g_test_add_func ("/waveloader/resize_sync", waveloader_resize_sync);
  g_test_add_func ("/waveloader/compare_load_resize", waveloader_compare_load_resize);

  return g_test_run ();
}
