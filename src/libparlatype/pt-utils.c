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

static gboolean PT_UTILS_UPGRADED;

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

/**
 * pt_utils_pretty_download_size:
 * @size: a size in bytes
 *
 * Rounds bytes to full KB, full MB or GB with one decimal (metric decimal prefixes, floor rounding).
 * E.g. 1000001 and 1999999 return "1 MB". Zero and negative values return "Unknown".
 *
 * Returns: (transfer full): a size string
 */
char *
pt_utils_pretty_download_size (int size)
{
  if (size <= 0)
    return g_strdup (_ ("Unknown"));

  if (size < 1000)
    size = 1000;

  if (size < 1000 * 1000)
    return g_strdup_printf (_ ("%d KB"), size / 1000);

  if (size < 1000 * 1000 * 1000)
    return g_strdup_printf (_ ("%d MB"), size / 1000 / 1000);

  return g_strdup_printf (_ ("%.1f GB"), (float) size / 1000 / 1000 / 1000);
}

void
_pt_utils_detect_upgrade (GSettings *settings)
{
  int major_version = g_settings_get_int (settings, "major-version");
  int minor_version = g_settings_get_int (settings, "minor-version");

  if (PT_MAJOR_VERSION > major_version ||
      (PT_MAJOR_VERSION == major_version && PT_MINOR_VERSION > minor_version))
    {
      PT_UTILS_UPGRADED = TRUE;
      g_settings_set_int (settings, "major-version", PT_MAJOR_VERSION);
      g_settings_set_int (settings, "minor-version", PT_MINOR_VERSION);
    }
  else
    {
      PT_UTILS_UPGRADED = FALSE;
    }
}

/**
 * pt_utils_upgraded:
 *
 * Reports whether the app has been upgraded, taking major and minor
 * version into account (without micro version).
 * This is done by comparing keys in GSettings with the current version.
 *
 * Returns: whether the app is running after an upgrade.
 */
gboolean
pt_utils_upgraded (void)
{
  return PT_UTILS_UPGRADED;
}

typedef struct
{
  char *source_folder;
  char *destination_folder;
} CopyData;

static void
copy_data_free (CopyData *copy_data)
{
  g_slice_free (CopyData, copy_data);
}

static void
copy_configs (GTask        *task,
              gpointer      source_object,
              gpointer      task_data,
              GCancellable *cancellable)
{
  CopyData        *copy_data = (CopyData *) task_data;
  gchar           *basename;
  gchar           *dest_path;
  GFile           *source;
  GFile           *dest;
  GFile           *sys_folder;
  GFileEnumerator *files;
  GError          *error = NULL;

  sys_folder = g_file_new_for_path (copy_data->source_folder);

  files = g_file_enumerate_children (sys_folder,
                                     G_FILE_ATTRIBUTE_STANDARD_NAME,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                     cancellable,
                                     &error);

  while (!error)
    {
      GFileInfo *info;
      if (!g_file_enumerator_iterate (files,
                                      &info,   /* no free necessary */
                                      &source, /* no free necessary */
                                      cancellable,
                                      &error))
        /* this is an error */
        break;

      if (!info)
        /* this is the end of enumeration */
        break;

      basename = g_file_get_basename (source);
      if (g_str_has_suffix (basename, ".asr"))
        {
          dest_path = g_build_path (G_DIR_SEPARATOR_S,
                                    copy_data->destination_folder,
                                    basename, NULL);
          dest = g_file_new_for_path (dest_path);
          g_file_copy (source,
                       dest,
                       G_FILE_COPY_TARGET_DEFAULT_PERMS,
                       cancellable,
                       NULL, /* progress callback      */
                       NULL, /* progress callback data */
                       &error);
          g_free (dest_path);
          g_object_unref (dest);

          if (error && g_error_matches (error, G_IO_ERROR,
                                        G_IO_ERROR_EXISTS))
            {
              g_clear_error (&error);
            }
        }
      g_free (basename);
    }

  g_clear_object (&files);
  g_object_unref (sys_folder);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * pt_utils_copy_configs_finish:
 * @result: the #GAsyncResult passed to your #GAsyncReadyCallback
 * @error:(nullable): a pointer to a NULL #GError, or NULL
 *
 * Finishes copy operation.
 *
 * Returns: true on success
 */
gboolean
pt_utils_copy_configs_finish (GAsyncResult *result,
                              GError      **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * pt_utils_copy_configs_async:
 * @source_folder: (transfer none): folder to copy files from
 * @destination_folder: folder to copy files to
 * @cancellable: (nullable): a #GCancellable or NULL
 * @callback: (scope async) (closure user_data): a #GAsyncReadyCallback to call when the operation is complete
 * @user_data: user_data for @callback
 *
 * Copy all files with extension `.asr` to another folder without
 * overwriting files.
 */
void
pt_utils_copy_configs_async (char               *source_folder,
                             char               *destination_folder,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
  GTask    *task;
  CopyData *copy_data;

  g_return_if_fail (source_folder != NULL);
  g_return_if_fail (destination_folder != NULL);

  task = g_task_new (NULL, cancellable, callback, user_data);
  copy_data = g_slice_new (CopyData);
  copy_data->source_folder = source_folder;
  copy_data->destination_folder = destination_folder;

  g_task_set_task_data (task, copy_data, (GDestroyNotify) copy_data_free);

  g_task_run_in_thread (task, copy_configs);
  g_object_unref (task);
}
