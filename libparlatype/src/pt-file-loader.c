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

/**
 * PtFileLoader:
 *
 * Loads text files.
 *
 * PtFileLoader reads the contents of an XML file using libxml and inserts them into a PtTextBuffer.
 */

// #define G_LOG_DOMAIN "pt-file-loader"

#include "config.h"

#include "pt-file-loader.h"

#include "pt-marshalers.h"
#include "pt-text-buffer.h"

#include <glib/gi18n.h>
#include <libxml/xmlreader.h>

#define CAST_BAD (gchar *)

struct _PtFileLoader
{
  GObject       parent_instance;
  PtTextView   *view;
  PtTextBuffer *buffer;
  GtkTextIter   pos;
  GFile        *file;
  gboolean      office_body;
  int           element;
  gboolean      first_paragraph;
};

enum
{
  PROP_0,
  PROP_VIEW,
  PROP_FILE,
  N_PROPS
};

enum
{
  UNKNOWN,
  META_AUDIO,
  TEXT_P,
  TEXT_S,
  SPAN_TIMESTAMP
};

enum
{
  INSERT_TIMESTAMP_BUTTON,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (PtFileLoader, pt_file_loader, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
error_handler (gpointer        user_data,
               const xmlError *error)
{
  /* https://gnome.pages.gitlab.gnome.org/libxml2/html/struct__xmlError.html
   * This is probably too detailed for users, messages are English only */

  g_printerr ("XML-ERROR\n");
  g_printerr ("domain : %d\n", error->domain);
  g_printerr ("code   : %d\n", error->code);
  g_printerr ("level  : %d\n", error->level);
  g_printerr ("message: %s\n", error->message);
  g_printerr ("file   : %s\n", error->file);
  g_printerr ("line   : %d\n", error->line);
  g_printerr ("str1   : %s\n", error->str1);
  g_printerr ("str2   : %s\n", error->str2);
  g_printerr ("str3   : %s\n", error->str3);
  g_printerr ("int1   : %d\n", error->int1);
  g_printerr ("int2   : %d\n", error->int2);
}

static void
debug_node (xmlTextReaderPtr reader)
{
  xmlChar *name, *value;

  name = xmlTextReaderName (reader);
  if (name == NULL)
    name = xmlStrdup (BAD_CAST "--");
  value = xmlTextReaderValue (reader);

  printf ("%d %d %s %d",
          xmlTextReaderDepth (reader),
          xmlTextReaderNodeType (reader),
          name,
          xmlTextReaderIsEmptyElement (reader));
  xmlFree (name);
  if (value == NULL)
    printf ("\n");
  else
    {
      printf (" »%s«\n", value);
      xmlFree (value);
    }
}

static void
process_element_node (xmlTextReaderPtr reader,
                      xmlChar         *name,
                      PtFileLoader    *self)
{
  xmlChar *attribute = NULL;

  /* Audio meta data */
  if (xmlStrEqual (name, BAD_CAST "meta:user-defined") &&
      xmlTextReaderMoveToAttribute (reader, BAD_CAST "meta:name") == 1)
    {
      attribute = xmlTextReaderValue (reader);
      if (xmlStrEqual (attribute, BAD_CAST "Parlatype"))
        {
          self->element = META_AUDIO;
        }
      goto out;
    }

  /* Ignore everything outside "office:body" */
  if (xmlStrEqual (name, BAD_CAST "office:body"))
    {
      self->office_body = TRUE;
      goto out;
    }

  if (!self->office_body)
    {
      goto out;
    }

  /* paragraph */
  if (xmlStrEqual (name, BAD_CAST "text:p"))
    {
      if (!self->first_paragraph)
        {
          /* close previous paragraph */
          gtk_text_buffer_insert (GTK_TEXT_BUFFER (self->buffer), &self->pos, "\n", 1);
        }
      self->first_paragraph = FALSE;

      self->element = TEXT_P;

      /* empty paragraph */
      if (xmlTextReaderIsEmptyElement (reader))
        {
          self->element = UNKNOWN;
        }
      goto out;
    }

  /* timestamp */
  if (xmlStrEqual (name, BAD_CAST "text:span") &&
      self->element == TEXT_P &&
      xmlTextReaderMoveToAttribute (reader, BAD_CAST "text:style-name") == 1)
    {
      attribute = xmlTextReaderValue (reader);
      if (xmlStrEqual (attribute, BAD_CAST "pt-timestamp"))
        {
          self->element = SPAN_TIMESTAMP;
        }
      goto out;
    }

  /* whitespace characters */
  if (xmlStrEqual (name, BAD_CAST "text:s"))
    {
      int num_whitespace = 1;
      if (xmlTextReaderMoveToAttribute (reader, BAD_CAST "text:c") == 1)
        {
          attribute = xmlTextReaderValue (reader);
          num_whitespace = atoi (CAST_BAD (attribute));
          if (num_whitespace < 1)
            num_whitespace = 1;
        }

      for (int n = 0; n < num_whitespace; n++)
        {
          gtk_text_buffer_insert (GTK_TEXT_BUFFER (self->buffer), &self->pos, " ", 1);
        }

      goto out;
    }

out:
  if (attribute != NULL)
    xmlFree (attribute);
}

static void
process_end_node (xmlTextReaderPtr reader,
                  xmlChar         *name,
                  PtFileLoader    *self)
{
  /* Check if in "office:body" */
  if (xmlStrEqual (name, BAD_CAST "office:body"))
    {
      self->office_body = FALSE;
      return;
    }

  /* closing paragraph */
  if (xmlStrEqual (name, BAD_CAST "text:p"))
    {
      self->element = UNKNOWN;
      return;
    }
}

static void
process_text_node (xmlTextReaderPtr reader,
                   xmlChar         *name,
                   PtFileLoader    *self)
{
  xmlChar *text = NULL;

  if (self->element == META_AUDIO)
    {
      GFile *audio_file;
      text = xmlTextReaderValue (reader);
      audio_file = g_file_new_for_uri (CAST_BAD text);
      pt_text_view_set_audio_file (self->view, audio_file);
      g_object_unref (audio_file);
      self->element = UNKNOWN;
      goto out;
    }

  /* Check if in "office:body" */
  if (!self->office_body)
    {
      goto out;
    }

  /* timestamp */
  if (self->element == SPAN_TIMESTAMP)
    {
      GtkTextChildAnchor *anchor;
      text = xmlTextReaderValue (reader);
      anchor = gtk_text_buffer_create_child_anchor (GTK_TEXT_BUFFER (self->buffer), &self->pos);
      g_signal_emit (self, signals[INSERT_TIMESTAMP_BUTTON], 0, anchor, atoi (CAST_BAD (text)));
      self->element = TEXT_P;
      goto out;
    }

  /* text in paragraph (including significant whitespace) */
  if (self->element == TEXT_P)
    {
      text = xmlTextReaderValue (reader);
      gtk_text_buffer_insert (GTK_TEXT_BUFFER (self->buffer), &self->pos, CAST_BAD text, -1);
      goto out;
    }

out:
  if (text != NULL)
    xmlFree (text);
}

static void
process_node (xmlTextReaderPtr reader,
              xmlChar         *name,
              PtFileLoader    *self)
{
  switch (xmlTextReaderNodeType (reader))
    {
    case XML_READER_TYPE_ELEMENT:
      process_element_node (reader, name, self);
      break;
    case XML_READER_TYPE_END_ELEMENT:
      process_end_node (reader, name, self);
      break;
    case XML_READER_TYPE_TEXT:
    case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
      process_text_node (reader, name, self);
      break;
    default:
        /* ignore other node types */;
    }
}

/**
 * pt_file_loader_load:
 * @self: a `PtFileLoader`
 * @error:
 *
 * Starts the actual loading. An async variant of this call didn't work well
 * and was removed.
 *
 * Returns: True on success
 */
gboolean
pt_file_loader_load (PtFileLoader *self,
                     GError      **error)
{
  xmlTextReaderPtr reader;
  xmlChar         *name;
  int              rc;
  gchar           *file;
  GtkTextIter      begin;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  file = g_file_get_path (self->file);
  if (!g_file_test (file, G_FILE_TEST_IS_REGULAR))
    {
      g_free (file);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _ ("Not a regular file"));
      return FALSE;
    }

  self->office_body = FALSE;
  self->first_paragraph = TRUE;
  reader = xmlNewTextReaderFilename (file);

  g_free (file);

  if (reader == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _ ("Error opening file"));
      return FALSE;
    }
  xmlTextReaderSetStructuredErrorHandler (reader, error_handler, self);
  /* empty buffer */
  gtk_text_buffer_begin_irreversible_action (GTK_TEXT_BUFFER (self->buffer));

  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (self->buffer), "", 0);
  gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self->buffer), FALSE);

  gtk_text_buffer_end_irreversible_action (GTK_TEXT_BUFFER (self->buffer));

  gtk_text_buffer_begin_irreversible_action (GTK_TEXT_BUFFER (self->buffer));
  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (self->buffer));

  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->buffer),
                                  &self->pos);

  rc = xmlTextReaderRead (reader);

  while (rc == 1)
    {
      if (!g_log_writer_default_would_drop (G_LOG_LEVEL_DEBUG, G_LOG_DOMAIN))
        {
          debug_node (reader);
        }

      name = xmlTextReaderName (reader);
      if (name != NULL)
        {
          process_node (reader, name, self);
          xmlFree (name);
        }

      rc = xmlTextReaderRead (reader);
    }

  xmlFreeTextReader (reader);

  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (self->buffer));
  gtk_text_buffer_end_irreversible_action (GTK_TEXT_BUFFER (self->buffer));

  if (rc != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _ ("Error reading file"));
      return FALSE;
    }

  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->buffer), &begin);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &begin, &begin);
  gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self->buffer), FALSE);

  return TRUE;
}

static void
pt_file_loader_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PtFileLoader *self = PT_FILE_LOADER (object);

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
      self->file = g_value_get_object (value);
      g_object_add_weak_pointer (G_OBJECT (self->file),
                                 (gpointer *) &self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_file_loader_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PtFileLoader *self = PT_FILE_LOADER (object);

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
pt_file_loader_dispose (GObject *object)
{
  PtFileLoader *self = PT_FILE_LOADER (object);

  if (self->view != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->view),
                                    (gpointer *) &self->view);

      self->buffer = NULL;
    }

  if (self->buffer != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->buffer),
                                    (gpointer *) &self->buffer);

      self->buffer = NULL;
    }

  if (self->file != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->file),
                                    (gpointer *) &self->file);

      self->file = NULL;
    }

  G_OBJECT_CLASS (pt_file_loader_parent_class)->dispose (object);
}

static void
pt_file_loader_class_init (PtFileLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = pt_file_loader_get_property;
  object_class->set_property = pt_file_loader_set_property;
  object_class->dispose = pt_file_loader_dispose;

  /**
   * PtFileLoader::insert-timestamp-button:
   * @self: the `PtFileLoader` emitting the signal
   * @anchor: GtkTextChildAnchor
   * @timestamp: a timestamp (int)
   *
   * Signal to insert a timestamp button in the view.
   */
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
                    G_TYPE_INT);

  /**
   * PtFileLoader:view:
   *
   * The view where contents are loaded to.
   */
  properties[PROP_VIEW] =
      g_param_spec_object ("view", NULL, NULL,
                           PT_TYPE_TEXT_VIEW,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtFileLoader:file:
   *
   * A text file in Parlatype format.
   */
  properties[PROP_FILE] =
      g_param_spec_object ("file", NULL, NULL,
                           G_TYPE_FILE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
pt_file_loader_init (PtFileLoader *self)
{
}

/**
 * pt_file_loader_new:
 * @view: the view where to insert file contents
 * @file: a text file
 *
 * Creates a new `PtFileLoader`. Use [method@Pt.FileLoader.load] to actually save the file.
 * Returns: a newly created `PtFileLoader`
 */
PtFileLoader *
pt_file_loader_new (PtTextView *view,
                    GFile      *file)
{
  g_return_val_if_fail (PT_IS_TEXT_VIEW (view), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (PT_TYPE_FILE_LOADER,
                       "view", view,
                       "file", file,
                       NULL);
}
