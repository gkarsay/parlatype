/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2004 Nokia Corporation
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

/* Includes lots of duplicated code from gtktextbuffer.c */

#include "config.h"

#include "pt-text-buffer.h"

#include "parlatype/parlatype.h"
#include "pt-marshalers.h"
#include "pt-timestamp-button.h"

struct _PtTextBuffer
{
  GtkTextBuffer parent;
  PtPlayer     *player;
};

enum
{
  INSERT_TIMESTAMP_BUTTON,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _ClipboardRequest ClipboardRequest;

struct _ClipboardRequest
{
  PtTextBuffer *buffer;
  guint         interactive : 1;
  guint         default_editable : 1;
  guint         replace_selection : 1;
};

G_DEFINE_TYPE (PtTextBuffer, pt_text_buffer, GTK_TYPE_TEXT_BUFFER)

static void
free_clipboard_request (ClipboardRequest *request_data)
{
  g_object_unref (request_data->buffer);
  g_free (request_data);
}

static gboolean
possibly_not_text (gunichar ch,
                   gpointer user_data)
{
  return ch == PT_TEXT_UNKNOWN_CHAR || ch == PT_TEXT_TIMESTAMP_DELIMITER;
}

static void
gtk_text_buffer_emit_insert (GtkTextBuffer *buffer,
                             GtkTextIter   *iter,
                             const char    *text,
                             int            len)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  g_return_if_fail (g_utf8_validate (text, len, NULL));

  if (len > 0)
    {
      g_signal_emit_by_name (buffer, "insert-text",
                             iter, text, len);
    }
}

static void
insert_text_range (GtkTextBuffer     *buffer,
                   GtkTextIter       *iter,
                   const GtkTextIter *orig_start,
                   const GtkTextIter *orig_end,
                   gboolean           interactive)
{
  char *text;

  text = gtk_text_iter_get_text (orig_start, orig_end);

  g_print ("insert text range: »%s«\n", text);

  gtk_text_buffer_emit_insert (buffer, iter, text, -1);

  g_free (text);
}

typedef struct _Range Range;
struct _Range
{
  GtkTextBuffer *buffer;
  GtkTextMark   *start_mark;
  GtkTextMark   *end_mark;
  GtkTextMark   *whole_end_mark;
  GtkTextIter   *range_start;
  GtkTextIter   *range_end;
  GtkTextIter   *whole_end;
};

static Range *
save_range (GtkTextIter *range_start,
            GtkTextIter *range_end,
            GtkTextIter *whole_end)
{
  Range *r;

  r = g_new (Range, 1);

  r->buffer = gtk_text_iter_get_buffer (range_start);
  g_object_ref (r->buffer);

  r->start_mark =
      gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                   NULL,
                                   range_start,
                                   FALSE);
  r->end_mark =
      gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                   NULL,
                                   range_end,
                                   TRUE);

  r->whole_end_mark =
      gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                   NULL,
                                   whole_end,
                                   TRUE);

  r->range_start = range_start;
  r->range_end = range_end;
  r->whole_end = whole_end;

  return r;
}

static void
restore_range (Range *r)
{
  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->range_start,
                                    r->start_mark);

  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->range_end,
                                    r->end_mark);

  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->whole_end,
                                    r->whole_end_mark);

  gtk_text_buffer_delete_mark (r->buffer, r->start_mark);
  gtk_text_buffer_delete_mark (r->buffer, r->end_mark);
  gtk_text_buffer_delete_mark (r->buffer, r->whole_end_mark);

  /* Due to the gravities on the marks, the ordering could have
   * gotten mangled; we switch to an empty range in that
   * case
   */

  if (gtk_text_iter_compare (r->range_start, r->range_end) > 0)
    *r->range_start = *r->range_end;

  if (gtk_text_iter_compare (r->range_end, r->whole_end) > 0)
    *r->range_end = *r->whole_end;

  g_object_unref (r->buffer);
  g_free (r);
}

static void
insert_range_untagged (GtkTextBuffer     *buffer,
                       GtkTextIter       *iter,
                       const GtkTextIter *orig_start,
                       const GtkTextIter *orig_end,
                       gboolean           interactive)
{
  GtkTextIter         range_start;
  GtkTextIter         range_end;
  GtkTextIter         start, end;
  GtkTextChildAnchor *anchor;
  guint               out_len;
  Range              *r;

  if (gtk_text_iter_equal (orig_start, orig_end))
    return;

  start = *orig_start;
  end = *orig_end;

  range_start = start;
  range_end = start;

  while (TRUE)
    {
      if (gtk_text_iter_equal (&range_start, &range_end))
        {
          /* Figure out how to move forward */

          g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);

          if (gtk_text_iter_equal (&range_end, &end))
            {
              /* nothing left to do */
              break;
            }
          else if (gtk_text_iter_get_char (&range_end) == PT_TEXT_UNKNOWN_CHAR)
            {
              GdkPaintable *paintable;

              paintable = gtk_text_iter_get_paintable (&range_end);
              anchor = gtk_text_iter_get_child_anchor (&range_end);

              if (paintable)
                {
                  r = save_range (&range_start,
                                  &range_end,
                                  &end);

                  gtk_text_buffer_insert_paintable (buffer, iter, paintable);

                  restore_range (r);
                  r = NULL;

                  gtk_text_iter_forward_char (&range_end);

                  range_start = range_end;
                }
              else if (anchor)
                {
                  /* Replace anchored timestamp button with timestamp as text */
                  GtkWidget **widgets;
                  const char *timestamp;

                  r = save_range (&range_start,
                                  &range_end,
                                  &end);

                  widgets = gtk_text_child_anchor_get_widgets (anchor, &out_len);
                  g_assert (out_len == 1);
                  timestamp = pt_timestamp_button_get_timestamp (PT_TIMESTAMP_BUTTON (widgets[0]));
                  g_print ("replacing timestamp button with %s\n", timestamp);
                  gtk_text_buffer_insert (buffer, iter, timestamp, -1);

                  restore_range (r);
                  r = NULL;

                  gtk_text_iter_forward_char (&range_end);
                  range_start = range_end;
                }
              else
                {
                  /* The GTK_TEXT_UNKNOWN_CHAR was in a text segment, so
                   * keep going.
                   */
                  gtk_text_iter_forward_find_char (&range_end,
                                                   possibly_not_text, NULL,
                                                   &end);

                  g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
                }
            }
          else if (gtk_text_iter_get_char (&range_end) == PT_TEXT_TIMESTAMP_DELIMITER)
            {
              char               *maybe_timestamp;
              GtkTextChildAnchor *new_anchor;

              gtk_text_iter_forward_find_char (&range_end,
                                               possibly_not_text, NULL,
                                               &end);

              g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
              if (gtk_text_iter_get_char (&range_end) == PT_TEXT_TIMESTAMP_DELIMITER)
                {
                  gtk_text_iter_forward_char (&range_end);
                  maybe_timestamp = gtk_text_iter_get_text (&range_start, &range_end);
                  if (pt_player_string_is_timestamp (PT_TEXT_BUFFER (buffer)->player, maybe_timestamp, FALSE))
                    {
                      g_print ("%s is a timestamp\n", maybe_timestamp);
                      r = save_range (&range_start,
                                      &range_end,
                                      &end);
                      new_anchor = gtk_text_buffer_create_child_anchor (buffer, iter);
                      g_signal_emit (buffer, signals[INSERT_TIMESTAMP_BUTTON], 0, new_anchor, maybe_timestamp);
                      restore_range (r);
                      r = NULL;

                      range_start = range_end;
                    }
                  else
                    {
                      g_print ("%s is NOT a timestamp\n", maybe_timestamp);
                      gtk_text_iter_backward_char (&range_end);
                    }
                  g_free (maybe_timestamp);
                }
              else
                {
                  range_start = range_end;
                }
            }
          else
            {
              /* Text segment starts here, so forward search to
               * find its possible endpoint
               */
              gtk_text_iter_forward_find_char (&range_end,
                                               possibly_not_text, NULL,
                                               &end);

              g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
            }
        }
      else
        {
          r = save_range (&range_start,
                          &range_end,
                          &end);

          insert_text_range (buffer,
                             iter,
                             &range_start,
                             &range_end,
                             interactive);

          restore_range (r);
          r = NULL;

          range_start = range_end;
        }
    }
}

static void
insert_range_not_inside_self (GtkTextBuffer     *buffer,
                              GtkTextIter       *iter,
                              const GtkTextIter *orig_start,
                              const GtkTextIter *orig_end,
                              gboolean           interactive)
{
  /* Find each range of uniformly-tagged text, insert it,
   * then apply the tags.
   */
  GtkTextIter start = *orig_start;
  GtkTextIter end = *orig_end;
  GtkTextIter range_start;
  GtkTextIter range_end;
  gboolean    insert_tags;

  if (gtk_text_iter_equal (orig_start, orig_end))
    return;

  insert_tags = gtk_text_buffer_get_tag_table (gtk_text_iter_get_buffer (orig_start)) == gtk_text_buffer_get_tag_table (buffer);

  gtk_text_iter_order (&start, &end);

  range_start = start;
  range_end = start;

  while (TRUE)
    {
      int         start_offset;
      GtkTextIter start_iter;
      GSList     *tags;
      GSList     *tmp_list;
      Range      *r;

      if (gtk_text_iter_equal (&range_start, &end))
        break; /* All done */

      g_assert (gtk_text_iter_compare (&range_start, &end) < 0);

      gtk_text_iter_forward_to_tag_toggle (&range_end, NULL);

      g_assert (!gtk_text_iter_equal (&range_start, &range_end));

      /* Clamp to the end iterator */
      if (gtk_text_iter_compare (&range_end, &end) > 0)
        range_end = end;

      /* We have a range with unique tags; insert it, and
       * apply all tags.
       */
      start_offset = gtk_text_iter_get_offset (iter);

      r = save_range (&range_start, &range_end, &end);

      insert_range_untagged (buffer, iter, &range_start, &range_end, interactive);

      restore_range (r);
      r = NULL;

      if (insert_tags)
        {
          gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start_offset);

          tags = gtk_text_iter_get_tags (&range_start);
          tmp_list = tags;
          while (tmp_list != NULL)
            {
              gtk_text_buffer_apply_tag (buffer, tmp_list->data, &start_iter, iter);
              tmp_list = tmp_list->next;
            }
          g_slist_free (tags);
        }

      range_start = range_end;
    }
}

static void
gtk_text_buffer_real_insert_range (GtkTextBuffer     *buffer,
                                   GtkTextIter       *iter,
                                   const GtkTextIter *orig_start,
                                   const GtkTextIter *orig_end,
                                   gboolean           interactive)
{
  GtkTextBuffer *src_buffer;

  /* Find each range of uniformly-tagged text, insert it,
   * then apply the tags.
   */
  if (gtk_text_iter_equal (orig_start, orig_end))
    return;

  if (interactive)
    gtk_text_buffer_begin_user_action (buffer);

  src_buffer = gtk_text_iter_get_buffer (orig_start);

  if (gtk_text_iter_get_buffer (iter) != src_buffer ||
      !gtk_text_iter_in_range (iter, orig_start, orig_end))
    {
      insert_range_not_inside_self (buffer, iter, orig_start, orig_end, interactive);
    }
  else
    {
      /* If you insert a range into itself, it could loop infinitely
       * because the region being copied keeps growing as we insert. So
       * we have to separately copy the range before and after
       * the insertion point.
       */
      GtkTextIter start = *orig_start;
      GtkTextIter end = *orig_end;
      GtkTextIter range_start;
      GtkTextIter range_end;
      Range      *first_half;
      Range      *second_half;

      gtk_text_iter_order (&start, &end);

      range_start = start;
      range_end = *iter;
      first_half = save_range (&range_start, &range_end, &end);

      range_start = *iter;
      range_end = end;
      second_half = save_range (&range_start, &range_end, &end);

      restore_range (first_half);
      insert_range_not_inside_self (buffer, iter, &range_start, &range_end, interactive);

      restore_range (second_half);
      insert_range_not_inside_self (buffer, iter, &range_start, &range_end, interactive);
    }

  if (interactive)
    gtk_text_buffer_end_user_action (buffer);
}

/**
 * gtk_text_buffer_insert_range:
 * @buffer: a `GtkTextBuffer`
 * @iter: a position in @buffer
 * @start: a position in a `GtkTextBuffer`
 * @end: another position in the same buffer as @start
 *
 * Copies text, tags, and paintables between @start and @end
 * and inserts the copy at @iter.
 *
 * The order of @start and @end doesn’t matter.
 *
 * Used instead of simply getting/inserting text because it preserves
 * images and tags. If @start and @end are in a different buffer from
 * @buffer, the two buffers must share the same tag table.
 *
 * Implemented via emissions of the ::insert-text and ::apply-tag signals,
 * so expect those.
 */
void
pt_text_buffer_insert_range (PtTextBuffer      *buffer,
                             GtkTextIter       *iter,
                             const GtkTextIter *start,
                             const GtkTextIter *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) ==
                    gtk_text_iter_get_buffer (end));
  // g_return_if_fail (gtk_text_iter_get_buffer (start)->priv->tag_table ==
  //                   buffer->priv->tag_table);
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == GTK_TEXT_BUFFER (buffer));

  gtk_text_buffer_real_insert_range (GTK_TEXT_BUFFER (buffer), iter, start, end, FALSE);
}

static PtTextBuffer *
create_clipboard_contents_buffer (GtkTextBuffer *buffer,
                                  GtkTextIter   *start_iter,
                                  GtkTextIter   *end_iter)
{
  GtkTextIter   start, end;
  PtTextBuffer *contents;

  contents = pt_text_buffer_new (gtk_text_buffer_get_tag_table (buffer));

  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (contents), &start, 0);
  pt_text_buffer_insert_range (contents, &start, start_iter, end_iter);
  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (contents), &start, &end);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (contents), &start, &end);

  /*  Ref the source buffer as long as the clipboard contents buffer
   *  exists, because it's needed for serializing the contents buffer.
   *  See http://bugzilla.gnome.org/show_bug.cgi?id=339195
   */
  g_object_ref (buffer);
  g_object_weak_ref (G_OBJECT (contents), (GWeakNotify) g_object_unref, buffer);

  return contents;
}

static void
cut_or_copy (GtkTextBuffer *buffer,
             GdkClipboard  *clipboard,
             gboolean       delete_region_after,
             gboolean       interactive,
             gboolean       default_editable)
{
  /* We prefer to cut the selected region between selection_bound and
   * insertion point. If that region is empty, then we cut the region
   * between the "anchor" and the insertion point (this is for
   * C-space and M-w and other Emacs-style copy/yank keys). Note that
   * insert and selection_bound are guaranteed to exist, but the
   * anchor only exists sometimes.
   */
  GtkTextIter start;
  GtkTextIter end;

  if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      /* Let's try the anchor thing */
      GtkTextMark *anchor = gtk_text_buffer_get_mark (buffer, "anchor");

      if (anchor == NULL)
        return;
      else
        {
          gtk_text_buffer_get_iter_at_mark (buffer, &end, anchor);
          gtk_text_iter_order (&start, &end);
        }
    }

  if (!gtk_text_iter_equal (&start, &end))
    {
      PtTextBuffer *contents;

      contents = create_clipboard_contents_buffer (buffer, &start, &end);
      gdk_clipboard_set (clipboard, PT_TYPE_TEXT_BUFFER, contents);
      g_object_unref (contents);

      if (delete_region_after)
        {
          if (interactive)
            gtk_text_buffer_delete_interactive (buffer, &start, &end,
                                                default_editable);
          else
            gtk_text_buffer_delete (buffer, &start, &end);
        }
    }
}

void
pt_text_buffer_cut_clipboard (PtTextBuffer *buffer,
                              GdkClipboard *clipboard,
                              gboolean      default_editable)
{
  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  cut_or_copy (GTK_TEXT_BUFFER (buffer), clipboard, TRUE, TRUE, default_editable);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

void
pt_text_buffer_copy_clipboard (PtTextBuffer *buffer,
                               GdkClipboard *clipboard)
{
  cut_or_copy (GTK_TEXT_BUFFER (buffer), clipboard, FALSE, TRUE, TRUE);
}

static void
get_paste_point (GtkTextBuffer *buffer,
                 GtkTextIter   *iter,
                 gboolean       clear_afterward)
{
  GtkTextIter  insert_point;
  GtkTextMark *paste_point_override;

  paste_point_override = gtk_text_buffer_get_mark (buffer,
                                                   "gtk_paste_point_override");

  if (paste_point_override != NULL)
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &insert_point,
                                        paste_point_override);
      if (clear_afterward)
        gtk_text_buffer_delete_mark (buffer, paste_point_override);
    }
  else
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &insert_point,
                                        gtk_text_buffer_get_insert (buffer));
    }

  *iter = insert_point;
}

static void
pre_paste_prep (ClipboardRequest *request_data,
                GtkTextIter      *insert_point)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (request_data->buffer);

  get_paste_point (buffer, insert_point, TRUE);

  if (request_data->replace_selection)
    {
      GtkTextIter start, end;

      if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
        {
          if (request_data->interactive)
            gtk_text_buffer_delete_interactive (GTK_TEXT_BUFFER (request_data->buffer),
                                                &start,
                                                &end,
                                                request_data->default_editable);
          else
            gtk_text_buffer_delete (GTK_TEXT_BUFFER (request_data->buffer), &start, &end);

          *insert_point = start;
        }
    }
}

static void
paste_from_buffer (GdkClipboard      *clipboard,
                   ClipboardRequest  *request_data,
                   PtTextBuffer      *src_buffer,
                   const GtkTextIter *start,
                   const GtkTextIter *end)
{
  GtkTextIter   insert_point;
  PtTextBuffer *buffer = request_data->buffer;

  /* We're about to emit a bunch of signals, so be safe */
  g_object_ref (src_buffer);

  /* Replacing the selection with itself */
  if (request_data->replace_selection &&
      buffer == src_buffer)
    {
      /* Clear the paste point if needed */
      get_paste_point (GTK_TEXT_BUFFER (buffer), &insert_point, TRUE);
      goto done;
    }

  if (request_data->interactive)
    gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

  pre_paste_prep (request_data, &insert_point);

  if (!gtk_text_iter_equal (start, end))
    {
      if (!request_data->interactive ||
          (gtk_text_iter_can_insert (&insert_point,
                                     request_data->default_editable)))
        gtk_text_buffer_real_insert_range (GTK_TEXT_BUFFER (buffer),
                                           &insert_point,
                                           start,
                                           end,
                                           request_data->interactive);
    }

done:
  g_signal_emit_by_name (buffer, "paste-done", clipboard);
  // emit_paste_done (buffer, clipboard);

  g_object_unref (src_buffer);

  free_clipboard_request (request_data);
}

static void
pt_text_buffer_paste_clipboard_finish (GObject      *source,
                                       GAsyncResult *result,
                                       gpointer      data)
{
  g_print ("%s\n", __func__);
  GdkClipboard     *clipboard = GDK_CLIPBOARD (source);
  ClipboardRequest *request_data = data;
  PtTextBuffer     *src_buffer;
  GtkTextIter       start, end;
  const GValue     *value;

  value = gdk_clipboard_read_value_finish (clipboard, result, NULL);
  if (value == NULL)
    return;

  src_buffer = g_value_get_object (value);

  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (src_buffer), &start, &end))
    paste_from_buffer (clipboard, request_data, src_buffer,
                       &start, &end);
}

void
pt_text_buffer_paste_clipboard (PtTextBuffer *buffer,
                                GdkClipboard *clipboard)
{
  ClipboardRequest *data = g_new (ClipboardRequest, 1);
  GtkTextIter       paste_point;
  GtkTextIter       start, end;

  data->buffer = g_object_ref (buffer);
  data->interactive = TRUE;
  data->default_editable = TRUE;

  /* When pasting with the cursor inside the selection area, you
   * replace the selection with the new text, otherwise, you
   * simply insert the new text at the point where the click
   * occurred, unselecting any selected text. The replace_selection
   * flag toggles this behavior.
   */
  data->replace_selection = FALSE;

  get_paste_point (GTK_TEXT_BUFFER (buffer), &paste_point, FALSE);
  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &start, &end) &&
      (gtk_text_iter_in_range (&paste_point, &start, &end) ||
       gtk_text_iter_equal (&paste_point, &end)))
    data->replace_selection = TRUE;

  gdk_clipboard_read_value_async (clipboard,
                                  PT_TYPE_TEXT_BUFFER,
                                  G_PRIORITY_DEFAULT,
                                  NULL,
                                  pt_text_buffer_paste_clipboard_finish,
                                  data);
}

static void
pt_text_buffer_deserialize_text_plain_finish (GObject      *source,
                                              GAsyncResult *result,
                                              gpointer      deserializer)
{
  g_print ("%s\n", __func__);
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GtkTextBuffer *buffer;
  GtkTextIter    start, end;
  GError        *error = NULL;
  gssize         written;
  gsize          size;
  char          *data;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  size = g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (
      g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (stream))));
  data = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (
      g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (stream))));

  if (data)
    {
      if (memchr (data, '\0', size))
        size = -1;
      buffer = g_value_get_object (gdk_content_deserializer_get_value (deserializer));
      gtk_text_buffer_get_end_iter (buffer, &end);
      gtk_text_buffer_insert (buffer, &end, data, size);
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      gtk_text_buffer_select_range (buffer, &start, &end);

      g_free (data);
    }

  gdk_content_deserializer_return_success (deserializer);
}

static void
pt_text_buffer_deserialize_text_plain (GdkContentDeserializer *deserializer)
{
  g_print ("%s\n", __func__);
  GOutputStream     *output, *filter;
  GCharsetConverter *converter;
  PtTextBuffer      *buffer;
  GError            *error = NULL;

  buffer = pt_text_buffer_new (NULL);
  g_value_set_object (gdk_content_deserializer_get_value (deserializer),
                      buffer);

  /* validates the stream */
  converter = g_charset_converter_new ("utf-8", "utf-8", &error);
  if (converter == NULL)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }
  g_charset_converter_set_use_fallback (converter, TRUE);

  output = g_memory_output_stream_new_resizable ();
  filter = g_converter_output_stream_new (output, G_CONVERTER (converter));
  g_object_unref (output);
  g_object_unref (converter);

  g_output_stream_splice_async (filter,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                pt_text_buffer_deserialize_text_plain_finish,
                                deserializer);
  g_object_unref (filter);
}

static void
pt_text_buffer_serialize_text_plain_finish (GObject      *source,
                                            GAsyncResult *result,
                                            gpointer      serializer)
{
  g_print ("%s\n", __func__);
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError        *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
pt_text_buffer_serialize_text_plain (GdkContentSerializer *serializer)
{
  g_print ("%s\n", __func__);
  PtTextBuffer *buffer;
  GtkTextIter   start, end;
  char         *str;

  buffer = g_value_get_object (gdk_content_serializer_get_value (serializer));

  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &start, &end))
    {
      str = gtk_text_iter_get_visible_text (&start, &end);
    }
  else
    {
      str = g_strdup ("");
    }
  g_print ("str: '%s'\n", str);
  gdk_content_serializer_set_task_data (serializer, str, g_free);

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   str,
                                   strlen (str),
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   pt_text_buffer_serialize_text_plain_finish,
                                   serializer);
}

static void
pt_text_buffer_register_serializers (void)
{
  g_print ("%s\n", __func__);
  gdk_content_register_deserializer ("text/plain;charset=utf-8",
                                     PT_TYPE_TEXT_BUFFER,
                                     pt_text_buffer_deserialize_text_plain,
                                     NULL,
                                     NULL);
  gdk_content_register_serializer (PT_TYPE_TEXT_BUFFER,
                                   "text/plain;charset=utf-8",
                                   pt_text_buffer_serialize_text_plain,
                                   NULL,
                                   NULL);
}

static void
pt_text_buffer_dispose (GObject *object)
{
  PtTextBuffer *self = PT_TEXT_BUFFER (object);

  g_clear_object (&self->player);

  G_OBJECT_CLASS (pt_text_buffer_parent_class)->dispose (object);
}

static void
pt_text_buffer_class_init (PtTextBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  // GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_text_buffer_dispose;
  pt_text_buffer_register_serializers ();

  signals[INSERT_TIMESTAMP_BUTTON] =
      g_signal_new ("insert-timestamp-button",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    _pt_cclosure_marshal_VOID__BOXED_STRING,
                    G_TYPE_NONE,
                    2,
                    GTK_TYPE_TEXT_CHILD_ANCHOR,
                    G_TYPE_STRING);
}

static void
pt_text_buffer_init (PtTextBuffer *self)
{
  self->player = pt_player_new ();
}

PtTextBuffer *
pt_text_buffer_new (GtkTextTagTable *tag_table)
{
  return g_object_new (PT_TYPE_TEXT_BUFFER,
                       "tag-table", tag_table,
                       NULL);
}
