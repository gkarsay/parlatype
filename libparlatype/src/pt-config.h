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

#pragma once

#if !defined(__PARLATYPE_H_INSIDE__) && !defined(PARLATYPE_COMPILATION)
#error "Only <parlatype.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define PT_TYPE_CONFIG (pt_config_get_type ())
G_DECLARE_DERIVABLE_TYPE (PtConfig, pt_config, PT, CONFIG, GObject)

struct _PtConfigClass
{
  GObjectClass parent_class;
};

/**
 * PT_ERROR:
 *
 * Error domain for Parlatype. Errors in this domain will be from the
 * #PtError enumeration. See #GError for more information on error domains.
 */
#define PT_ERROR pt_error_quark ()

/**
 * PtError:
 * @PT_ERROR_PLUGIN_MISSING_PROPERTY: The plugin doesn’t have a property.
 * @PT_ERROR_PLUGIN_NOT_WRITABLE: The plugin’s property is not writable.
 * @PT_ERROR_PLUGIN_WRONG_VALUE: The value is not valid for the property.
 *
 * Error codes for Parlatype in the PT_ERROR domain.
 */
typedef enum
{
  PT_ERROR_PLUGIN_MISSING_PROPERTY,
  PT_ERROR_PLUGIN_NOT_WRITABLE,
  PT_ERROR_PLUGIN_WRONG_VALUE,
} PtError;

GQuark pt_error_quark (void);

gchar *pt_config_get_name (PtConfig *self);
gboolean pt_config_set_name (PtConfig *self,
                             gchar *name);
gchar *pt_config_get_base_folder (PtConfig *self);
gboolean pt_config_set_base_folder (PtConfig *self,
                                    gchar *name);
gchar *pt_config_get_plugin (PtConfig *self);
gchar *pt_config_get_lang_code (PtConfig *self);
gchar *pt_config_get_lang_name (PtConfig *self);
gchar *pt_config_get_key (PtConfig *self,
                          gchar *key);

GFile *pt_config_get_file (PtConfig *self);
void pt_config_set_file (PtConfig *self,
                         GFile *file);

gboolean pt_config_apply (PtConfig *self,
                          GObject *plugin,
                          GError **error);
gboolean pt_config_is_valid (PtConfig *self);
gboolean pt_config_is_installed (PtConfig *self);
PtConfig *pt_config_new (GFile *file);

G_END_DECLS
