/* Copyright 2026 Gabor Karsay <gabor.karsay@gmx.at>
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
 *
 * Parts of this program are taken from or inspired by GStreamer’s gstplay.c.
 */

/**
 * PtTimestamp:
 *
 * Timestamp utilities
 */

#define ONE_HOUR 3600000
#define TEN_MINUTES 600000

#include "config.h"

#include "pt-timestamp.h"

#include <glib/gi18n.h>

/**
 * pt_timestamp_to_string:
 * @time: time in milliseconds to converse
 * @duration: duration of stream in milliseconds (max time)
 * @precision: a #PtPrecisionType
 *
 * Returns the given time as a string for display to the user. Format type is
 * determined by @duration, e.g. if duration is long format, it returns a string
 * in long format, too.
 *
 * Returns: (transfer full): the time string
 *
 * Since: 1.4
 */
char *
pt_timestamp_to_string (int             time,
                        int             duration,
                        PtPrecisionType precision)
{
  /* Don’t assert time <= duration because duration is not exact */

  g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

  char *result;
  int   h, m, s, ms, mod;

  h = time / 3600000;
  mod = time % 3600000;
  m = mod / 60000;
  ms = time % 60000;
  s = ms / 1000;
  ms = ms % 1000;

  /* Short or long format depends on total duration */
  if (duration >= ONE_HOUR || time >= ONE_HOUR)
    {
      switch (precision)
        {
        case PT_PRECISION_SECOND:
          /* Translators: This is a time format, like "2:05:30" for 2
             hours, 5 minutes, and 30 seconds. You may change ":" to
             the separator that your locale uses or use "%Id" instead
             of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("long time format", "%d:%02d:%02d"), h, m, s);
          break;
        case PT_PRECISION_SECOND_10TH:
          /* Translators: This is a time format, like "2:05:30.1" for 2
             hours, 5 minutes, 30 seconds, and 1 tenthsecond. You may
             change ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("long time format, 1 digit", "%d:%02d:%02d.%d"), h, m, s, ms / 100);
          break;
        case PT_PRECISION_SECOND_100TH:
          /* Translators: This is a time format, like "2:05:30.12" for 2
             hours, 5 minutes, 30 seconds, and 12 hundrethseconds. You may
             change ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("long time format, 2 digits", "%d:%02d:%02d.%02d"), h, m, s, ms / 10);
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }

      return result;
    }

  if (duration >= TEN_MINUTES)
    {
      switch (precision)
        {
        case PT_PRECISION_SECOND:
          /* Translators: This is a time format, like "05:30" for
             5 minutes, and 30 seconds. You may change ":" to
             the separator that your locale uses or use "%I02d" instead
             of "%02d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("short time format", "%02d:%02d"), m, s);
          break;
        case PT_PRECISION_SECOND_10TH:
          /* Translators: This is a time format, like "05:30.1" for
             5 minutes, 30 seconds, and 1 tenthsecond. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("short time format, 1 digit", "%02d:%02d.%d"), m, s, ms / 100);
          break;
        case PT_PRECISION_SECOND_100TH:
          /* Translators: This is a time format, like "05:30.12" for
             5 minutes, 30 seconds, and 12 hundrethseconds. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("short time format, 2 digits", "%02d:%02d.%02d"), m, s, ms / 10);
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }
    }
  else
    {
      switch (precision)
        {
        case PT_PRECISION_SECOND:
          /* Translators: This is a time format, like "5:30" for
             5 minutes, and 30 seconds. You may change ":" to
             the separator that your locale uses or use "%Id" instead
             of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("shortest time format", "%d:%02d"), m, s);
          break;
        case PT_PRECISION_SECOND_10TH:
          /* Translators: This is a time format, like "05:30.1" for
             5 minutes, 30 seconds, and 1 tenthsecond. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("shortest time format, 1 digit", "%d:%02d.%d"), m, s, ms / 100);
          break;
        case PT_PRECISION_SECOND_100TH:
          /* Translators: This is a time format, like "05:30.12" for
             5 minutes, 30 seconds, and 12 hundrethseconds. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("shortest time format, 2 digits", "%d:%02d.%02d"), m, s, ms / 10);
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }
    }

  return result;
}

/**
 * pt_timestamp_from_string:
 * @timestamp: a timestamp string
 *
 * Returns the time in milliseconds represented by the timestamp or -1 for
 * invalid timestamps.
 *
 * Returns: the time in milliseconds represented by the timestamp or -1
 * for invalid timestamps
 *
 * Since: 1.6
 */
int
pt_timestamp_from_string (char *timestamp)
{
  g_return_val_if_fail (timestamp != NULL, -1);

  int      h, m, s, ms, result;
  char    *cmp; /* timestamp without delimiters */
  gboolean long_format;
  gboolean fraction;
  char   **split = NULL;
  uint     n_split;

  /* Check for formal validity */
  if (!g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9]:[0-9][0-9][\\.|\\-][0-9][0-9]?[#|\\)|\\]]?$", timestamp, 0, 0) && !g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9][\\.|\\-][0-9][0-9]?[#|\\)|\\]]?$", timestamp, 0, 0) && !g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9]:[0-9][0-9][#|\\)|\\]]?$", timestamp, 0, 0) && !g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9][#|\\)|\\]]?$", timestamp, 0, 0))
    {
      return -1;
    }

  /* Delimiters must match */
  if (g_str_has_prefix (timestamp, "#") && !g_str_has_suffix (timestamp, "#"))
    return -1;
  if (g_str_has_prefix (timestamp, "(") && !g_str_has_suffix (timestamp, ")"))
    return -1;
  if (g_str_has_prefix (timestamp, "[") && !g_str_has_suffix (timestamp, "]"))
    return -1;
  if (g_regex_match_simple ("^[0-9]", timestamp, 0, 0))
    {
      if (!g_regex_match_simple ("[0-9]$", timestamp, 0, 0))
        return -1;
    }

  /* Remove delimiters */
  if (g_str_has_prefix (timestamp, "#") || g_str_has_prefix (timestamp, "(") || g_str_has_prefix (timestamp, "["))
    {
      timestamp++;
      cmp = g_strdup_printf ("%.*s", (int) strlen (timestamp) - 1, timestamp);
    }
  else
    {
      cmp = g_strdup (timestamp);
    }

  /* Determine format and split timestamp into h, m, s, ms */
  h = 0;
  ms = 0;

  long_format = g_regex_match_simple (":[0-9][0-9]:", cmp, 0, 0);
  fraction = !g_regex_match_simple (".*:[0-9][0-9]$", cmp, 0, 0);
  split = g_regex_split_simple ("[:|\\.|\\-]", cmp, 0, 0);
  g_free (cmp);
  if (!split)
    return -1;

  n_split = 2;
  if (long_format)
    n_split += 1;
  if (fraction)
    n_split += 1;
  if (n_split != g_strv_length (split))
    {
      g_strfreev (split);
      return -1;
    }

  if (long_format)
    {
      h = (int) g_ascii_strtoull (split[0], NULL, 10);
      m = (int) g_ascii_strtoull (split[1], NULL, 10);
      s = (int) g_ascii_strtoull (split[2], NULL, 10);
      if (fraction)
        {
          ms = (int) g_ascii_strtoull (split[3], NULL, 10);
          if (strlen (split[3]) == 1)
            ms = ms * 100;
          else
            ms = ms * 10;
        }
    }
  else
    {
      m = (int) g_ascii_strtoull (split[0], NULL, 10);
      s = (int) g_ascii_strtoull (split[1], NULL, 10);
      if (fraction)
        {
          ms = (int) g_ascii_strtoull (split[2], NULL, 10);
          if (strlen (split[2]) == 1)
            ms = ms * 100;
          else
            ms = ms * 10;
        }
    }

  g_strfreev (split);

  /* Sanity check */
  if (s > 59 || m > 59)
    return -1;

  result = (h * 3600 + m * 60 + s) * 1000 + ms;

  return result;
}
