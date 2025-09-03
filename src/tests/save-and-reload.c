/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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

#include "pt-text-view.h"

#include <glib.h>
#include <gtk/gtk.h>

#define RESET "reset-text"

typedef struct
{
  GMainLoop     *loop;
  GFile         *file;
  PtTextView    *view;
  GtkTextBuffer *buffer;
  char          *text;
} TestData;

static void
compare_buffer_content_with_string (GtkTextBuffer *buffer,
                                    char          *string)
{
  GtkTextIter start, end;
  gchar      *content;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  content = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
  g_assert_cmpstr (content, ==, string);
}

static void
save_cb (GObject      *source_object,
         GAsyncResult *res,
         gpointer      user_data)
{
  PtTextView *view = PT_TEXT_VIEW (source_object);
  TestData   *test_data = user_data;
  GError     *error = NULL;
  gboolean    success;

  success = pt_text_view_save_finish (view, res, &error);
  g_assert_true (success);
  g_assert_null (error);

  gtk_text_buffer_set_text (test_data->buffer, RESET, -1);
  compare_buffer_content_with_string (test_data->buffer, RESET);

  success = pt_text_view_load (view, test_data->file, &error);
  g_assert_true (success);
  g_assert_null (error);

  compare_buffer_content_with_string (test_data->buffer, test_data->text);

  g_main_loop_quit (test_data->loop);
}

static void
save_and_reload (char *filename,
                 char *text)
{
  TestData     *test_data;
  GMainContext *context;
  char         *path;

  path = g_test_build_filename (G_TEST_BUILT, filename, NULL);
  context = g_main_context_default ();

  test_data = g_new (TestData, 1);
  test_data->file = g_file_new_for_path (path);
  test_data->view = pt_text_view_new ();
  test_data->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (test_data->view));
  test_data->loop = g_main_loop_new (context, FALSE);
  test_data->text = text;

  gtk_text_buffer_set_text (test_data->buffer, text, -1);
  compare_buffer_content_with_string (test_data->buffer, text);

  pt_text_view_save_async (test_data->view, test_data->file, NULL, save_cb, test_data);

  g_main_loop_run (test_data->loop);

  g_free (path);
  g_object_unref (test_data->view);
  g_object_unref (test_data->file);
  g_main_loop_unref (test_data->loop);
  g_slice_free (TestData, test_data);
}

static void
empty (void)
{
  save_and_reload ("test01.pt", "");
}

static void
newline (void)
{
  save_and_reload ("test02.pt", "\n");
}

static void
three_newlines (void)
{
  save_and_reload ("test03.pt", "\n\n\n");
}

static void
one_space (void)
{
  save_and_reload ("test04.pt", " ");
}

static void
two_spaces (void)
{
  save_and_reload ("test05.pt", "  ");
}

static void
three_spaces (void)
{
  save_and_reload ("test06.pt", "   ");
}

static void
paragraphs_with_spaces (void)
{
  save_and_reload ("test07.pt", "   a   \n     b   c   \n  d ");
}

static void
emojis (void)
{
  save_and_reload ("test08.pt", "😍\n😎");
}

static void
xml_entities (void)
{
  save_and_reload ("test09.pt", "&amp; &lt; &gt; &apos; &quot; & < > ' \"");
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/save-and-reload/empty", empty);
  g_test_add_func ("/save-and-reload/newline", newline);
  g_test_add_func ("/save-and-reload/three_newlines", three_newlines);
  g_test_add_func ("/save-and-reload/one-space", one_space);
  g_test_add_func ("/save-and-reload/two-spaces", two_spaces);
  g_test_add_func ("/save-and-reload/three-spaces", three_spaces);
  g_test_add_func ("/save-and-reload/paragraphs-with-spaces", paragraphs_with_spaces);
  g_test_add_func ("/save-and-reload/emojis", emojis);
  g_test_add_func ("/save-and-reload/xml-entities", xml_entities);
  gtk_init ();

  return g_test_run ();
}
