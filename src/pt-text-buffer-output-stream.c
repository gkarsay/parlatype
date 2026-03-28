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

#include "pt-text-buffer-output-stream.h"

struct _PtTextBufferOutputStream
{
  GOutputStream parent_instance;
  PtTextBuffer *buffer;
  gboolean      is_initialized;
  gboolean      is_closed;
  gchar        *data;
  gsize         buflen;
  GtkTextIter   pos;
};

enum
{
  PROP_0,
  PROP_BUFFER
};

G_DEFINE_TYPE (PtTextBufferOutputStream, pt_text_buffer_output_stream, G_TYPE_OUTPUT_STREAM)

static gssize
pt_text_buffer_output_stream_write (GOutputStream *stream,
                                    const void    *buffer,
                                    gsize          count,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  PtTextBufferOutputStream *self = PT_TEXT_BUFFER_OUTPUT_STREAM (stream);
  gchar                    *text;
  gsize                     len;
  gboolean                  freetext = FALSE;
  gssize                    ret = -1;
  GtkTextIter              *iter;

  iter = &self->pos;

  if (self->buffer == NULL)
    {
      return ret;
    }

  if (g_cancellable_set_error_if_cancelled (cancellable, error) ||
      self->buffer == NULL)
    {
      return ret;
    }

  if (!self->is_initialized)
    {
      /* Begin not undoable action. Begin also a normal user action,
       * since we load the file chunk by chunk and it should be seen
       * as only one action, for the features that rely on the user
       * action.
       */
      gtk_text_buffer_begin_irreversible_action (GTK_TEXT_BUFFER (self->buffer));
      gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (self->buffer));

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->buffer),
                                      &self->pos);

      self->is_initialized = TRUE;
    }

  if (self->buflen > 0)
    {
      len = self->buflen + count;
      text = g_malloc (len + 1);

      memcpy (text, self->data, self->buflen);
      memcpy (text + self->buflen, buffer, count);

      text[len] = '\0';

      g_free (self->data);

      self->data = NULL;
      self->buflen = 0;

      freetext = TRUE;
    }
  else
    {
      text = (gchar *) buffer;
      len = count;
    }

  gtk_text_buffer_insert (GTK_TEXT_BUFFER (self->buffer), iter, buffer, len);
  // validate_and_insert (self, text, len, freetext);

  if (freetext)
    {
      g_free (text);
    }

  return (gssize) count;
}

static gboolean
pt_text_buffer_output_stream_flush (GOutputStream *stream,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  PtTextBufferOutputStream *self = PT_TEXT_BUFFER_OUTPUT_STREAM (stream);

  if (self->is_closed ||
      self->buffer == NULL)
    {
      return TRUE;
    }

  return TRUE;
}

static gboolean
pt_text_buffer_output_stream_close (GOutputStream *stream,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  PtTextBufferOutputStream *self = PT_TEXT_BUFFER_OUTPUT_STREAM (stream);

  if (!self->is_closed && self->is_initialized)
    {
      // end_append_text_to_document (self);
      self->is_closed = TRUE;
    }

  if (self->buflen > 0)
    {
      /*g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _ ("Incomplete UTF-8 sequence in input"));*/
      g_printerr ("Incomplete UTF-8 sequence in input");

      return FALSE;
    }

  return TRUE;
}

static void
pt_text_buffer_output_stream_constructed (GObject *object)
{
  PtTextBufferOutputStream *self = PT_TEXT_BUFFER_OUTPUT_STREAM (object);

  gtk_text_buffer_begin_irreversible_action (GTK_TEXT_BUFFER (self->buffer));

  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (self->buffer), "", 0);
  gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self->buffer), FALSE);

  gtk_text_buffer_end_irreversible_action (GTK_TEXT_BUFFER (self->buffer));

  G_OBJECT_CLASS (pt_text_buffer_output_stream_parent_class)->constructed (object);
}

static void
pt_text_buffer_output_stream_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  PtTextBufferOutputStream *self = PT_TEXT_BUFFER_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_assert (self->buffer == NULL);
      self->buffer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_text_buffer_output_stream_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  PtTextBufferOutputStream *self = PT_TEXT_BUFFER_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_text_buffer_output_stream_dispose (GObject *object)
{
  PtTextBufferOutputStream *self = PT_TEXT_BUFFER_OUTPUT_STREAM (object);

  g_clear_object (&self->buffer);

  G_OBJECT_CLASS (pt_text_buffer_output_stream_parent_class)->dispose (object);
}

static void
pt_text_buffer_output_stream_class_init (PtTextBufferOutputStreamClass *klass)
{
  GObjectClass       *object_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);

  object_class->get_property = pt_text_buffer_output_stream_get_property;
  object_class->set_property = pt_text_buffer_output_stream_set_property;
  object_class->constructed = pt_text_buffer_output_stream_constructed;
  object_class->dispose = pt_text_buffer_output_stream_dispose;

  stream_class->write_fn = pt_text_buffer_output_stream_write;
  stream_class->close_fn = pt_text_buffer_output_stream_close;
  stream_class->flush = pt_text_buffer_output_stream_flush;

  g_object_class_install_property (object_class,
                                   PROP_BUFFER,
                                   g_param_spec_object ("buffer", NULL, NULL,
                                                        PT_TYPE_TEXT_BUFFER,
                                                        G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_STATIC_STRINGS));
}

static void
pt_text_buffer_output_stream_init (PtTextBufferOutputStream *self)
{
  self->is_initialized = FALSE;
  self->is_closed = FALSE;
}

PtTextBufferOutputStream *
pt_text_buffer_output_stream_new (PtTextBuffer *buffer)
{
  g_return_val_if_fail (PT_IS_TEXT_BUFFER (buffer), NULL);

  return g_object_new (PT_TYPE_TEXT_BUFFER_OUTPUT_STREAM,
                       "buffer", buffer,
                       NULL);
}
