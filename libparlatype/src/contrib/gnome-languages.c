/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2008  Red Hat, Inc,
 *           2007  William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by : William Jon McCann <mccann@jhu.edu>
 *              Ray Strode <rstrode@redhat.com>
 */

#include "config.h"

#include <glib.h>
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "gnome-languages.h"

#define ISO_CODES_DATADIR ISO_CODES_PREFIX "/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX "/share/locale"

static GHashTable *gnome_languages_map;
static GHashTable *gnome_territories_map;

/**
 * gnome_parse_locale:
 * @locale: a locale string
 * @language_codep: (out) (optional) (transfer full): location to
 * store the language code, or %NULL
 * @country_codep: (out) (optional) (nullable) (transfer full): location to
 * store the country code, or %NULL
 * @codesetp: (out) (optional) (nullable) (transfer full): location to
 * store the codeset, or %NULL
 * @modifierp: (out) (optional) (nullable) (transfer full): location to
 * store the modifier, or %NULL
 *
 * Extracts the various components of a locale string of the form
 * [language[_country][.codeset][@modifier]]. See
 * http://en.wikipedia.org/wiki/Locale.
 *
 * Return value: %TRUE if parsing was successful.
 *
 * Since: 3.8
 */
gboolean
gnome_parse_locale (const char *locale,
                    char **language_codep,
                    char **country_codep,
                    char **codesetp,
                    char **modifierp)
{
  static GRegex *re = NULL;
  GMatchInfo *match_info;
  gboolean res;
  gboolean retval;

  match_info = NULL;
  retval = FALSE;

  if (re == NULL)
    {
      GError *error = NULL;
      re = g_regex_new ("^(?P<language>[^_.@[:space:]]+)"
                        "(_(?P<territory>[[:upper:]]+))?"
                        "(\\.(?P<codeset>[-_0-9a-zA-Z]+))?"
                        "(@(?P<modifier>[[:ascii:]]+))?$",
                        0, 0, &error);
      if (re == NULL)
        {
          g_warning ("%s", error->message);
          g_error_free (error);
          goto out;
        }
    }

  if (!g_regex_match (re, locale, 0, &match_info) ||
      g_match_info_is_partial_match (match_info))
    {
      g_warning ("locale '%s' isn't valid\n", locale);
      goto out;
    }

  res = g_match_info_matches (match_info);
  if (!res)
    {
      g_warning ("Unable to parse locale: %s", locale);
      goto out;
    }

  retval = TRUE;

  if (language_codep != NULL)
    {
      *language_codep = g_match_info_fetch_named (match_info, "language");
    }

  if (country_codep != NULL)
    {
      *country_codep = g_match_info_fetch_named (match_info, "territory");

      if (*country_codep != NULL &&
          *country_codep[0] == '\0')
        {
          g_free (*country_codep);
          *country_codep = NULL;
        }
    }

  if (codesetp != NULL)
    {
      *codesetp = g_match_info_fetch_named (match_info, "codeset");

      if (*codesetp != NULL &&
          *codesetp[0] == '\0')
        {
          g_free (*codesetp);
          *codesetp = NULL;
        }
    }

  if (modifierp != NULL)
    {
      *modifierp = g_match_info_fetch_named (match_info, "modifier");

      if (*modifierp != NULL &&
          *modifierp[0] == '\0')
        {
          g_free (*modifierp);
          *modifierp = NULL;
        }
    }

out:
  g_match_info_free (match_info);

  return retval;
}

static gboolean
is_fallback_language (const char *code)
{
  const char *fallback_language_names[] = { "C", "POSIX", NULL };
  int i;

  for (i = 0; fallback_language_names[i] != NULL; i++)
    {
      if (strcmp (code, fallback_language_names[i]) == 0)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static const char *
get_language (const char *code)
{
  const char *name;
  int len;

  g_assert (code != NULL);

  if (is_fallback_language (code))
    {
      return "Unspecified";
    }

  len = strlen (code);
  if (len != 2 && len != 3)
    {
      return NULL;
    }

  name = (const char *) g_hash_table_lookup (gnome_languages_map, code);

  return name;
}

static char *
get_first_item_in_semicolon_list (const char *list)
{
  char **items;
  char *item;

  /* Some entries in iso codes have multiple values, separated
   * by semicolons.  Not really sure which one to pick, so
   * we just arbitrarily pick the first one.
   */
  items = g_strsplit (list, "; ", 2);

  item = g_strdup (items[0]);
  g_strfreev (items);

  return item;
}

static char *
capitalize_utf8_string (const char *str)
{
  char first[8] = { 0 };

  if (!str)
    return NULL;

  g_unichar_to_utf8 (g_unichar_totitle (g_utf8_get_char (str)), first);

  return g_strconcat (first, g_utf8_offset_to_pointer (str, 1), NULL);
}

static char *
get_translated_language (const char *code,
                         const char *locale)
{
  const char *language;
  char *name;

  language = get_language (code);

  name = NULL;
  if (language != NULL)
    {
      const char *translated_name;

      if (is_fallback_language (code))
        {
          name = g_strdup (_ ("Unspecified"));
        }
      else
        {
          g_autofree char *tmp = NULL;
          if (strlen (code) == 2)
            {
              translated_name = dgettext ("iso_639", language);
            }
          else
            {
              translated_name = dgettext ("iso_639_3", language);
            }
          tmp = get_first_item_in_semicolon_list (translated_name);
          name = capitalize_utf8_string (tmp);
        }
    }

  return name;
}

static const char *
get_territory (const char *code)
{
  const char *name;
  int len;

  g_assert (code != NULL);

  len = strlen (code);
  if (len != 2 && len != 3)
    {
      return NULL;
    }

  name = (const char *) g_hash_table_lookup (gnome_territories_map, code);

  return name;
}

static char *
get_translated_territory (const char *code,
                          const char *locale)
{
  const char *territory;
  char *name;

  territory = get_territory (code);

  name = NULL;
  if (territory != NULL)
    {
      const char *translated_territory;
      g_autofree char *tmp = NULL;

      translated_territory = dgettext ("iso_3166", territory);
      tmp = get_first_item_in_semicolon_list (translated_territory);
      name = capitalize_utf8_string (tmp);
    }

  return name;
}

static void
languages_parse_start_tag (GMarkupParseContext *ctx,
                           const char *element_name,
                           const char **attr_names,
                           const char **attr_values,
                           gpointer user_data,
                           GError **error)
{
  const char *ccode_longB;
  const char *ccode_longT;
  const char *ccode;
  const char *ccode_id;
  const char *lang_common_name;
  const char *lang_name;

  if (!(g_str_equal (element_name, "iso_639_entry") || g_str_equal (element_name, "iso_639_3_entry")) || attr_names == NULL || attr_values == NULL)
    {
      return;
    }

  ccode = NULL;
  ccode_longB = NULL;
  ccode_longT = NULL;
  ccode_id = NULL;
  lang_common_name = NULL;
  lang_name = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "iso_639_1_code"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              if (strlen (*attr_values) != 2)
                {
                  return;
                }
              ccode = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "iso_639_2B_code"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                {
                  return;
                }
              ccode_longB = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "iso_639_2T_code"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                {
                  return;
                }
              ccode_longT = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "id"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              if (strlen (*attr_values) != 2 &&
                  strlen (*attr_values) != 3)
                {
                  return;
                }
              ccode_id = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "common_name"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              lang_common_name = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "name"))
        {
          lang_name = *attr_values;
        }

      ++attr_names;
      ++attr_values;
    }

  if (lang_common_name != NULL)
    {
      lang_name = lang_common_name;
    }

  if (lang_name == NULL)
    {
      return;
    }

  if (ccode != NULL)
    {
      g_hash_table_insert (gnome_languages_map,
                           g_strdup (ccode),
                           g_strdup (lang_name));
    }
  if (ccode_longB != NULL)
    {
      g_hash_table_insert (gnome_languages_map,
                           g_strdup (ccode_longB),
                           g_strdup (lang_name));
    }
  if (ccode_longT != NULL)
    {
      g_hash_table_insert (gnome_languages_map,
                           g_strdup (ccode_longT),
                           g_strdup (lang_name));
    }
  if (ccode_id != NULL)
    {
      g_hash_table_insert (gnome_languages_map,
                           g_strdup (ccode_id),
                           g_strdup (lang_name));
    }
}

static void
territories_parse_start_tag (GMarkupParseContext *ctx,
                             const char *element_name,
                             const char **attr_names,
                             const char **attr_values,
                             gpointer user_data,
                             GError **error)
{
  const char *acode_2;
  const char *acode_3;
  const char *ncode;
  const char *territory_common_name;
  const char *territory_name;

  if (!g_str_equal (element_name, "iso_3166_entry") || attr_names == NULL || attr_values == NULL)
    {
      return;
    }

  acode_2 = NULL;
  acode_3 = NULL;
  ncode = NULL;
  territory_common_name = NULL;
  territory_name = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "alpha_2_code"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              if (strlen (*attr_values) != 2)
                {
                  return;
                }
              acode_2 = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "alpha_3_code"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                {
                  return;
                }
              acode_3 = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "numeric_code"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              if (strlen (*attr_values) != 3)
                {
                  return;
                }
              ncode = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "common_name"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              territory_common_name = *attr_values;
            }
        }
      else if (g_str_equal (*attr_names, "name"))
        {
          territory_name = *attr_values;
        }

      ++attr_names;
      ++attr_values;
    }

  if (territory_common_name != NULL)
    {
      territory_name = territory_common_name;
    }

  if (territory_name == NULL)
    {
      return;
    }

  if (acode_2 != NULL)
    {
      g_hash_table_insert (gnome_territories_map,
                           g_strdup (acode_2),
                           g_strdup (territory_name));
    }
  if (acode_3 != NULL)
    {
      g_hash_table_insert (gnome_territories_map,
                           g_strdup (acode_3),
                           g_strdup (territory_name));
    }
  if (ncode != NULL)
    {
      g_hash_table_insert (gnome_territories_map,
                           g_strdup (ncode),
                           g_strdup (territory_name));
    }
}

static void
languages_variant_init (const char *variant)
{
  gboolean res;
  gsize buf_len;
  g_autofree char *buf = NULL;
  g_autofree char *filename = NULL;
  g_autoptr (GError) error = NULL;

  bindtextdomain (variant, ISO_CODES_LOCALESDIR);
  bind_textdomain_codeset (variant, "UTF-8");

  error = NULL;
  filename = g_strdup_printf (ISO_CODES_DATADIR "/%s.xml", variant);
  res = g_file_get_contents (filename,
                             &buf,
                             &buf_len,
                             &error);
  if (res)
    {
      g_autoptr (GMarkupParseContext) ctx = NULL;
      GMarkupParser parser = { languages_parse_start_tag, NULL, NULL, NULL, NULL };

      ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

      error = NULL;
      res = g_markup_parse_context_parse (ctx, buf, buf_len, &error);

      if (!res)
        {
          g_warning ("Failed to parse '%s': %s\n",
                     filename,
                     error->message);
        }
    }
  else
    {
      g_warning ("Failed to load '%s': %s\n",
                 filename,
                 error->message);
    }
}

static void
languages_init (void)
{
  if (gnome_languages_map)
    return;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  gnome_languages_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  languages_variant_init ("iso_639");
  languages_variant_init ("iso_639_3");
}

static void
territories_init (void)
{
  gboolean res;
  gsize buf_len;
  g_autofree char *buf = NULL;
  g_autoptr (GError) error = NULL;

  if (gnome_territories_map)
    return;

  bindtextdomain ("iso_3166", ISO_CODES_LOCALESDIR);
  bind_textdomain_codeset ("iso_3166", "UTF-8");

  gnome_territories_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  error = NULL;
  res = g_file_get_contents (ISO_CODES_DATADIR "/iso_3166.xml",
                             &buf,
                             &buf_len,
                             &error);
  if (res)
    {
      g_autoptr (GMarkupParseContext) ctx = NULL;
      GMarkupParser parser = { territories_parse_start_tag, NULL, NULL, NULL, NULL };

      ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

      error = NULL;
      res = g_markup_parse_context_parse (ctx, buf, buf_len, &error);

      if (!res)
        {
          g_warning ("Failed to parse '%s': %s\n",
                     ISO_CODES_DATADIR "/iso_3166.xml",
                     error->message);
        }
    }
  else
    {
      g_warning ("Failed to load '%s': %s\n",
                 ISO_CODES_DATADIR "/iso_3166.xml",
                 error->message);
    }
}

/**
 * gnome_get_language_from_locale:
 * @locale: a locale string
 * @translation: (nullable): a locale string
 *
 * Gets the language description for @locale. If @translation is
 * provided the returned string is translated accordingly.
 *
 * Return value: (transfer full): the language description. Caller
 * takes ownership.
 *
 * Since: 3.8
 */
char *
gnome_get_language_from_locale (const char *locale,
                                const char *translation)
{
  GString *full_language;
  g_autofree char *language_code = NULL;
  g_autofree char *territory_code = NULL;
  g_autofree char *codeset_code = NULL;
  g_autofree char *translated_language = NULL;
  g_autofree char *translated_territory = NULL;

  g_return_val_if_fail (locale != NULL, NULL);
  g_return_val_if_fail (*locale != '\0', NULL);

  full_language = g_string_new (NULL);

  languages_init ();
  territories_init ();

  gnome_parse_locale (locale,
                      &language_code,
                      &territory_code,
                      &codeset_code,
                      NULL);

  if (language_code == NULL)
    {
      goto out;
    }

  translated_language = get_translated_language (language_code, translation);
  if (translated_language == NULL)
    {
      goto out;
    }

  full_language = g_string_append (full_language, translated_language);

  if (territory_code != NULL)
    {
      translated_territory = get_translated_territory (territory_code, translation);
    }
  if (translated_territory != NULL)
    {
      g_string_append_printf (full_language,
                              " (%s)",
                              translated_territory);
    }

out:
  if (full_language->len == 0)
    {
      g_string_free (full_language, TRUE);
      return NULL;
    }

  return g_string_free (full_language, FALSE);
}
