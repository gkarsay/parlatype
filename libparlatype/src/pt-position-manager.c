/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
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
 * pt-position-manager
 * Saves and loads last playback position for a file.
 *
 * The current implementation is using g_file_set_attribute() from GIO, which
 * to my knowledge works only with GVFS. The GVFS daemon saves custom
 * attributes in ~/.local/share/gvfs-metadata in binary format.
 *
 * The manager is a class/object although that is not necessary at the moment.
 * It doesn't have public or private attributes but this could change if other
 * backends would be supported (e.g. SQlite based).
 */

#include "config.h"

#include "pt-position-manager.h"

#include <gio/gio.h>

#define METADATA_POSITION "metadata::parlatype::position"

struct _PtPositionManager
{
  GObject parent;
};

G_DEFINE_TYPE (PtPositionManager, pt_position_manager, G_TYPE_OBJECT)

/**
 * pt_position_manager_save:
 * @self: a #PtPositionManager
 * @file: #GFile holding the file
 * @pos: position to save in milliseconds
 *
 * Tries to save the given position for the given file.
 * Success and failure are logged at info level.
 */
void
pt_position_manager_save (PtPositionManager *self,
                          GFile             *file,
                          gint64             pos)
{
  GError    *error = NULL;
  GFileInfo *info;
  gchar      value[64];

  if (!file)
    return;

  info = g_file_info_new ();
  g_snprintf (value, sizeof (value), "%" G_GINT64_FORMAT, pos);

  g_file_info_set_attribute_string (info, METADATA_POSITION, value);

  g_file_set_attributes_from_info (
      file,
      info,
      G_FILE_QUERY_INFO_NONE,
      NULL,
      &error);

  if (error)
    {
      /* There are valid cases were setting attributes is not
       * possible, e.g. in sandboxed environments, containers etc.
       * Use G_LOG_LEVEL_INFO because other log levels go to stderr
       * and might result in failed tests. */
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "MESSAGE",
                        "Position not saved: %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                        "MESSAGE", "Position saved");
    }

  g_object_unref (info);
}

/**
 * pt_position_manager_load:
 * @self: a #PtPositionManager
 * @file: #GFile holding the file
 *
 * Tries to get the position where the given file ended the last time.
 * Success is logged at info level, "soft" failure (no position saved) is
 * not logged and other "hard" failures are logged at warning level.
 *
 * Return value: Position in milliseconds or zero on failure.
 * */
gint64
pt_position_manager_load (PtPositionManager *self,
                          GFile             *file)
{
  GError    *error = NULL;
  GFileInfo *info;
  gchar     *value = NULL;
  gint64     pos = 0;

  if (!file)
    return 0;

  info = g_file_query_info (file, METADATA_POSITION,
                            G_FILE_QUERY_INFO_NONE, NULL, &error);
  if (error)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "MESSAGE",
                        "Metadata not retrieved: %s", error->message);
      g_error_free (error);
      return 0;
    }

  value = g_file_info_get_attribute_as_string (info, METADATA_POSITION);
  if (value)
    {
      pos = g_ascii_strtoull (value, NULL, 0);
      g_free (value);

      if (pos > 0)
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                            "MESSAGE", "Metadata: last known "
                                       "position %" G_GINT64_FORMAT " ms",
                            pos);
        }
    }

  g_object_unref (info);
  return pos;
}

/* --------------------- Init and GObject management ------------------------ */

static void
pt_position_manager_init (PtPositionManager *self)
{
}

static void
pt_position_manager_class_init (PtPositionManagerClass *klass)
{
}

PtPositionManager *
pt_position_manager_new (void)
{
  return g_object_new (PT_TYPE_POSITION_MANAGER, NULL);
}
