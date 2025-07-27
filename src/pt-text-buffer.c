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

#include "pt-text-buffer.h"

struct _PtTextBuffer
{
  GtkTextBuffer parent;
};

G_DEFINE_TYPE (PtTextBuffer, pt_text_buffer, GTK_TYPE_TEXT_BUFFER)

static void
pt_text_buffer_class_init (PtTextBufferClass *class)
{
  /*GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_text_buffer_dispose;*/
}

static void
pt_text_buffer_init (PtTextBuffer *content)
{
}

PtTextBuffer *
pt_text_buffer_new (void)
{
  return g_object_new (PT_TYPE_TEXT_BUFFER, NULL);
}
