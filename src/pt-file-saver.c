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

#define G_LOG_DOMAIN "pt-file-saver"

#include "config.h"

#include "pt-file-saver.h"

#include "pt-timestamp-button.h"

#include <glib/gi18n.h>
#include <libxml/xmlwriter.h>

#define CAST_BAD (gchar *)

/* We don’t have meaningful error messages and fail on every error. */

#define LOG_WRITER_ERROR                                                \
  if (rc < 0)                                                           \
    {                                                                   \
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,             \
                        "MESSAGE", "xmlwriter error: %s", G_STRLOC);    \
      g_task_return_new_error (task, G_FILE_ERROR, G_FILE_ERROR_FAILED, \
                               _ ("Error saving document."));           \
      return;                                                           \
    }

struct _PtFileSaver
{
  GObject       parent_instance;
  PtTextView   *view;
  PtTextBuffer *buffer;
  GFile        *file;
};

enum
{
  PROP_0,
  PROP_VIEW,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_TYPE (PtFileSaver, pt_file_saver, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static int
add_text (xmlTextWriterPtr writer,
          GString         *string)
{
  int rc = 0;

  if (string->len > 0)
    {
      rc = xmlTextWriterWriteString (writer, BAD_CAST string->str);
      string = g_string_erase (string, 0, string->len);
    }

  return rc;
}

static int
add_space_element (xmlTextWriterPtr writer,
                   GString         *string,
                   int              num_whitespace)
{
  int rc;

  rc = add_text (writer, string);
  if (rc < 0)
    return rc;

  rc = xmlTextWriterStartElement (writer, BAD_CAST "text:s");
  if (rc < 0)
    return rc;

  if (num_whitespace > 2)
    {
      gchar *number = g_strdup_printf ("%d", num_whitespace - 1);

      rc = xmlTextWriterWriteAttribute (writer, BAD_CAST "text:c",
                                        BAD_CAST number);

      g_free (number);
    }
  if (rc < 0)
    return rc;

  return xmlTextWriterEndElement (writer); /* text:s */
}

static int
add_indentation (xmlTextWriterPtr writer,
                 int              indent)
{
  if (indent > 10)
    indent = 10;

  return xmlTextWriterWriteRawLen (writer, BAD_CAST "\n          ", indent + 1);
}

static void
pt_file_saver_save_thread (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  PtFileSaver *self = PT_FILE_SAVER (source_object);

  /* xmlWriter */
  xmlTextWriterPtr writer;
  char            *filename;
  int              rc = 0;
  int              indent = 0;

  /* Buffer processing */
  GtkTextIter start, end;
  GString    *string;
  char       *character;
  int         num_whitespace = 0;
  gboolean    iterate = TRUE;
  guint       out_len;

  const GFile *audio;
  char        *audio_url;

  filename = g_file_get_path (self->file);
  writer = xmlNewTextWriterFilename (filename, 0);
  g_free (filename);

  if (writer == NULL)
    rc = -1;
  LOG_WRITER_ERROR

  rc = xmlTextWriterStartDocument (writer, NULL, "UTF-8", NULL);
  LOG_WRITER_ERROR

  rc = xmlTextWriterStartElementNS (writer,
                                    BAD_CAST "office",
                                    BAD_CAST "document",
                                    BAD_CAST "urn:oasis:names:tc:opendocument:xmlns:office:1.0");
  LOG_WRITER_ERROR

  rc = xmlTextWriterWriteAttribute (writer,
                                    BAD_CAST "xmlns:text",
                                    BAD_CAST "urn:oasis:names:tc:opendocument:xmlns:text:1.0");
  LOG_WRITER_ERROR

  rc = xmlTextWriterWriteAttribute (writer,
                                    BAD_CAST "xmlns:meta",
                                    BAD_CAST "urn:oasis:names:tc:opendocument:xmlns:meta:1.0");
  LOG_WRITER_ERROR

  rc = xmlTextWriterWriteAttribute (writer,
                                    BAD_CAST "office:mimetype",
                                    BAD_CAST "application/vnd.oasis.opendocument.text");
  LOG_WRITER_ERROR

  rc = add_indentation (writer, ++indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterStartElement (writer, BAD_CAST "office:meta");
  LOG_WRITER_ERROR

  rc = add_indentation (writer, ++indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterWriteFormatElement (writer, BAD_CAST "meta:generator",
                                        "%s/%s", PACKAGE_NAME, PACKAGE_VERSION);
  LOG_WRITER_ERROR

  g_assert (PT_IS_TEXT_VIEW (self->view));
  audio = pt_text_view_get_audio_file (self->view);
  if (audio != NULL)
    {
      g_assert (G_IS_FILE (audio));
      audio_url = g_file_get_uri ((void *) audio);

      rc = add_indentation (writer, indent);
      LOG_WRITER_ERROR

      rc = xmlTextWriterStartElement (writer, BAD_CAST "meta:user-defined");
      LOG_WRITER_ERROR
      rc = xmlTextWriterWriteAttribute (writer,
                                        BAD_CAST "meta:name",
                                        BAD_CAST "Parlatype");
      LOG_WRITER_ERROR
      rc = xmlTextWriterWriteString (writer, BAD_CAST audio_url);
      LOG_WRITER_ERROR

      rc = xmlTextWriterEndElement (writer);
      LOG_WRITER_ERROR
      g_free (audio_url);
    }

  rc = add_indentation (writer, --indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterEndElement (writer); /* office:meta */
  LOG_WRITER_ERROR

  rc = add_indentation (writer, indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterStartElement (writer, BAD_CAST "office:body");
  LOG_WRITER_ERROR

  rc = add_indentation (writer, ++indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterStartElement (writer, BAD_CAST "office:text");
  LOG_WRITER_ERROR

  rc = add_indentation (writer, ++indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterStartElement (writer, BAD_CAST "text:p");
  LOG_WRITER_ERROR

  string = g_string_new (NULL);
  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->buffer),
                                  &start);
  end = start;

  while (iterate)
    {
      iterate = gtk_text_iter_forward_char (&end);
      character = gtk_text_iter_get_slice (&start, &end);

      /* Space characters need extra treatment: Consecutive spaces are
       * added as a <text:s> element except for the first character.
       * If the element has more than one character its attribute "text:c"
       * denotes the number of characters. */

      if (g_strcmp0 (character, " ") == 0)
        {
          num_whitespace++;

          /* The first space is not a sequence yet. It will be added to the text. */

          if (num_whitespace > 1)
            {
              if (iterate)
                {
                  /* In a space sequence, accumulate their number */
                  start = end;
                  g_free (character);
                  continue;
                }
              else
                {
                  /* This is the last character in the buffer, process it */
                  rc = add_space_element (writer, string, num_whitespace);
                  LOG_WRITER_ERROR

                  g_free (character);
                  break;
                }
            }
        }
      else
        {
          if (num_whitespace > 1)
            {
              /* There is a sequence which ended now */
              rc = add_space_element (writer, string, num_whitespace);
              LOG_WRITER_ERROR

              num_whitespace = 0;
            }
        }

      if (g_strcmp0 (character, "\n") == 0)
        {
          /* Add accumulated text, end paragraph and start the next paragraph */

          rc = add_text (writer, string);
          LOG_WRITER_ERROR

          rc = xmlTextWriterEndElement (writer); /* text:p */
          LOG_WRITER_ERROR

          rc = add_indentation (writer, indent);
          LOG_WRITER_ERROR
          rc = xmlTextWriterStartElement (writer, BAD_CAST "text:p");
          LOG_WRITER_ERROR
        }
      else
        {
          /* Accumulate text */

          if (g_strcmp0 (character, "￼") == 0) /* Unicode character 0xFFFC */
            {
              /* Convert timestamp button to <text:span> element */

              GtkTextChildAnchor *anchor;
              anchor = gtk_text_iter_get_child_anchor (&start);
              if (anchor != NULL)
                {
                  GtkWidget **widgets;
                  const char *timestamp;
                  widgets = gtk_text_child_anchor_get_widgets (anchor, &out_len);
                  timestamp = pt_timestamp_button_get_timestamp (PT_TIMESTAMP_BUTTON (widgets[0]));

                  rc = add_text (writer, string);
                  LOG_WRITER_ERROR

                  rc = xmlTextWriterStartElement (writer, BAD_CAST "text:span");
                  LOG_WRITER_ERROR

                  rc = xmlTextWriterWriteAttribute (writer,
                                                    BAD_CAST "text:style-name",
                                                    BAD_CAST "pt-timestamp");
                  LOG_WRITER_ERROR

                  rc = xmlTextWriterWriteString (writer, BAD_CAST timestamp);
                  LOG_WRITER_ERROR

                  rc = xmlTextWriterEndElement (writer);
                  LOG_WRITER_ERROR
                }
            }
          else
            {
              g_string_append (string, character);
            }
          g_free (character);
        }
      start = end;
    }

  rc = add_text (writer, string);
  LOG_WRITER_ERROR

  g_string_free (string, TRUE);

  rc = xmlTextWriterEndElement (writer); /* text:p */
  LOG_WRITER_ERROR

  rc = add_indentation (writer, --indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterEndElement (writer); /* office:text */
  LOG_WRITER_ERROR

  rc = add_indentation (writer, --indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterEndElement (writer); /* office:body */
  LOG_WRITER_ERROR

  rc = add_indentation (writer, --indent);
  LOG_WRITER_ERROR
  rc = xmlTextWriterEndElement (writer); /* office:document */
  LOG_WRITER_ERROR

  rc = xmlTextWriterEndDocument (writer);
  LOG_WRITER_ERROR

  xmlFreeTextWriter (writer);

  g_task_return_boolean (task, TRUE);
}

gboolean
pt_file_saver_save_finish (PtFileSaver  *self,
                           GAsyncResult *result,
                           GError      **error)
{
  gboolean ok;

  g_return_val_if_fail (PT_IS_FILE_SAVER (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  ok = g_task_propagate_boolean (G_TASK (result), error);

  if (ok && self->buffer != NULL)
    {
      gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self->buffer),
                                    FALSE);
    }

  return ok;
}

void
pt_file_saver_save_async (PtFileSaver        *self,
                          gint                io_priority,
                          GCancellable       *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer            user_data)
{
  GTask *task;

  g_return_if_fail (PT_IS_FILE_SAVER (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->buffer != NULL);
  g_return_if_fail (self->file != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, NULL, NULL);
  g_task_set_priority (task, io_priority);
  g_task_run_in_thread (task, pt_file_saver_save_thread);
  g_object_unref (task);
}

static void
pt_file_saver_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  PtFileSaver *self = PT_FILE_SAVER (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_assert (self->view == NULL);
      self->view = g_value_get_object (value);
      g_object_add_weak_pointer (G_OBJECT (self->view),
                                 (gpointer *) &self->view);
      self->buffer = PT_TEXT_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));
      g_object_add_weak_pointer (G_OBJECT (self->buffer),
                                 (gpointer *) &self->buffer);
      break;

    case PROP_FILE:
      g_assert (self->file == NULL);
      self->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_file_saver_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PtFileSaver *self = PT_FILE_SAVER (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_file_saver_dispose (GObject *object)
{
  PtFileSaver *self = PT_FILE_SAVER (object);

  if (self->view != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->view),
                                    (gpointer *) &self->view);

      self->view = NULL;
    }

  if (self->buffer != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->buffer),
                                    (gpointer *) &self->buffer);

      self->buffer = NULL;
    }

  g_clear_object (&self->file);

  G_OBJECT_CLASS (pt_file_saver_parent_class)->dispose (object);
}

static void
pt_file_saver_class_init (PtFileSaverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = pt_file_saver_get_property;
  object_class->set_property = pt_file_saver_set_property;
  object_class->dispose = pt_file_saver_dispose;

  properties[PROP_VIEW] =
      g_param_spec_object ("view", NULL, NULL,
                           PT_TYPE_TEXT_VIEW,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_FILE] =
      g_param_spec_object ("file", NULL, NULL,
                           G_TYPE_FILE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
pt_file_saver_init (PtFileSaver *self)
{
}

PtFileSaver *
pt_file_saver_new (PtTextView *view, GFile *file)
{
  g_return_val_if_fail (PT_IS_TEXT_VIEW (view), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (PT_TYPE_FILE_SAVER,
                       "view", view,
                       "file", file,
                       NULL);
}
