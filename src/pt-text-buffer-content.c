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

#include "pt-text-buffer-content.h"

struct _PtTextBufferContent
{
  GdkContentProvider parent;

  PtTextBuffer *text_buffer;
};

G_DEFINE_TYPE (PtTextBufferContent, pt_text_buffer_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
pt_text_buffer_content_ref_formats (GdkContentProvider *provider)
{
  PtTextBufferContent *content = PT_TEXT_BUFFER_CONTENT (provider);

  if (content->text_buffer)
    return gdk_content_formats_new_for_gtype (PT_TYPE_TEXT_BUFFER);
  else
    return gdk_content_formats_new (NULL, 0);
}

static gboolean
pt_text_buffer_content_get_value (GdkContentProvider *provider,
                                  GValue             *value,
                                  GError            **error)
{
  PtTextBufferContent *content = PT_TEXT_BUFFER_CONTENT (provider);

  if (G_VALUE_HOLDS (value, PT_TYPE_TEXT_BUFFER) &&
      content->text_buffer != NULL)
    {
      g_value_set_object (value, content->text_buffer);
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (pt_text_buffer_content_parent_class)->get_value (provider, value, error);
}

static void
pt_text_buffer_content_detach (GdkContentProvider *provider,
                               GdkClipboard       *clipboard)
{
  PtTextBufferContent *content = PT_TEXT_BUFFER_CONTENT (provider);
  GtkTextIter          insert;
  GtkTextIter          selection_bound;

  if (content->text_buffer == NULL)
    return;

  /* Move selection_bound to the insertion point */
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (content->text_buffer), &insert,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (content->text_buffer)));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (content->text_buffer), &selection_bound,
                                    gtk_text_buffer_get_selection_bound (GTK_TEXT_BUFFER (content->text_buffer)));

  if (!gtk_text_iter_equal (&insert, &selection_bound))
    gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (content->text_buffer),
                               gtk_text_buffer_get_selection_bound (GTK_TEXT_BUFFER (content->text_buffer)),
                               &insert);
}

static void
pt_text_buffer_content_class_init (PtTextBufferContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  provider_class->ref_formats = pt_text_buffer_content_ref_formats;
  provider_class->get_value = pt_text_buffer_content_get_value;
  provider_class->detach_clipboard = pt_text_buffer_content_detach;
}

static void
pt_text_buffer_content_init (PtTextBufferContent *content)
{
}

GdkContentProvider *
pt_text_buffer_content_new (PtTextBuffer *buffer)
{
  PtTextBufferContent *content;

  content = g_object_new (PT_TYPE_TEXT_BUFFER_CONTENT, NULL);
  content->text_buffer = g_object_ref (buffer);

  return GDK_CONTENT_PROVIDER (content);
}
