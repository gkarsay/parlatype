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

#include "pt-utils.h"

#include <glib/gi18n.h>

/**
 * pt_file_filters_new:
 * @first_filter_type: a PtFilter
 * @...: other PtFilters, ended with NULL
 *
 * Variadic method to create file filters.
 *
 * Returns: (transfer full): a G_LIST_MODEL
 */
GListModel *
pt_file_filters_new (int first_filter_type, ...)
{
  GListStore *result;
  int         filter_type;
  va_list     args;

  result = g_list_store_new (GTK_TYPE_FILE_FILTER);

  va_start (args, first_filter_type);

  filter_type = first_filter_type;
  while (filter_type != PT_FILTER_NONE)
    {
      GtkFileFilter *filter = gtk_file_filter_new ();

      switch (filter_type)
        {
        case PT_FILTER_ALL:
          gtk_file_filter_set_name (filter, _ ("All Files"));
          gtk_file_filter_add_pattern (filter, "*");
          break;
        case PT_FILTER_AUDIO:
          gtk_file_filter_set_name (filter, _ ("Audio Files"));
          gtk_file_filter_add_mime_type (filter, "audio/*");
          break;
        case PT_FILTER_TEXT:
          gtk_file_filter_set_name (filter, _ ("Parlatype Files"));
          gtk_file_filter_add_pattern (filter, "*.pt");
          break;
        case PT_FILTER_OPEN_DOCUMENT:
          gtk_file_filter_set_name (filter, _ ("Open Document Flat Files"));
          gtk_file_filter_add_mime_type (filter, "application/vnd.oasis.opendocument.text-flat-xml");
          break;

        default:
          g_assert_not_reached ();
        }

      g_list_store_append (result, filter);

      filter_type = va_arg (args, int);
    }

  va_end (args);

  return G_LIST_MODEL (result);
}
