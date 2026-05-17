/* Copyright 2021 Gabor Karsay <gabor.karsay@gmx.at>
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
 * PtConfig:
 *
 * Configuration for ASR plugins
 *
 * A PtConfig represents a configuration for a GStreamer ASR plugin.
 * Configuration files are written in .ini-like style. Each file holds one
 * configuration that is used to instantiate a PtConfig.
 *
 * Its main use is to be applied to a GStreamer plugin.
 *
 * PtConfig is used for reading configurations in, not for writing them.
 * To add your own configuration, edit it yourself. Learn more about the file
 * format in the description of #GKeyFile in general and in
 * pt_config_is_valid() in particular.
 *
 * To delete a configuration, get its #PtConfig:file property, delete the file
 * and unref the config object.
 */

#define GETTEXT_PACKAGE GETTEXT_LIB

#include "config.h"

#include "pt-config.h"

#include "contrib/gnome-languages.h"
#include "pt-download.h"
#include "pt-downloader.h"
#include "pt-speech-recognizer.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

struct _PtConfig
{
  GObject parent_instance;

  /* Immutable properties */
  gchar    *id;
  gchar    *path;
  GFile    *file;
  GKeyFile *keyfile;
  gchar    *lang_name;
  gchar    *lang_code;
  gchar    *plugin;
  gchar    *downloader;
  int       major_version;
  int       minor_version;
  gboolean  is_valid;

  /* Directly changeable by user */
  gchar *name;
  gchar *base_folder;

  /* Indirectly changeable by user */
  gboolean    is_installed;
  PtDownload *download;

  /* Set externally */
  gboolean active;
  gboolean downloadable;
};

enum
{
  PROP_FILE = 1,
  PROP_IS_VALID,
  PROP_IS_INSTALLED,
  PROP_ACTIVE,
  PROP_DOWNLOADABLE,
  PROP_NAME,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

enum
{
  SIGNAL_DOWNLOAD_FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gboolean pt_config_verify_install (PtConfig *self);

G_DEFINE_FINAL_TYPE (PtConfig, pt_config, G_TYPE_OBJECT)

/**
 * pt_error_quark:
 *
 * Registers an error quark for Parlatype.
 *
 * Returns: the error quark
 */

GQuark
pt_error_quark (void)
{
  return g_quark_from_static_string ("pt-error-quark");
}

static gboolean
pt_config_save (PtConfig *self)
{
  GError  *error = NULL;
  gboolean result;

  result = g_key_file_save_to_file (
      self->keyfile,
      self->path,
      &error);

  if (error)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Keyfile not saved: %s", error->message);
      g_error_free (error);
    }

  return result;
}

static GValue
pt_config_get_value (PtConfig *self,
                     gchar    *group,
                     gchar    *key,
                     GType     type)
{
  GError *error = NULL;
  GValue  result = G_VALUE_INIT;

  g_assert (g_strcmp0 (group, "Files") == 0 ||
            g_strcmp0 (group, "Parameters") == 0 ||
            g_strcmp0 (group, "Download") == 0);

  if (g_strcmp0 (group, "Files") == 0)
    {
      g_value_init (&result, G_TYPE_STRING);
      gchar  *base = pt_config_get_base_folder (self);
      gchar **pieces = g_key_file_get_string_list (self->keyfile,
                                                   "Files", key, NULL, &error);
      gchar  *relative = g_build_filenamev (pieces);
      gchar  *absolute = g_build_filename (base, relative, NULL);
      g_value_set_string (&result, absolute);
      g_free (absolute);
      g_free (relative);
      g_strfreev (pieces);
    }
  else
    {
      switch (type)
        {
        case (G_TYPE_STRING):
          g_value_init (&result, G_TYPE_STRING);
          gchar *string = g_key_file_get_string (self->keyfile, group, key, &error);
          g_value_set_string (&result, string);
          g_free (string);
          break;
        case (G_TYPE_UINT):
        case (G_TYPE_INT):
          g_value_init (&result, G_TYPE_INT);
          gint integer = g_key_file_get_integer (self->keyfile, group, key, &error);
          g_value_set_int (&result, integer);
          break;
        case (G_TYPE_FLOAT):
          g_value_init (&result, G_TYPE_FLOAT);
          gfloat floaty = g_key_file_get_double (self->keyfile, group, key, &error);
          g_value_set_float (&result, floaty);
          break;
        case (G_TYPE_DOUBLE):
          g_value_init (&result, G_TYPE_DOUBLE);
          gdouble doubly = g_key_file_get_double (self->keyfile, group, key, &error);
          g_value_set_double (&result, doubly);
          break;
        case (G_TYPE_BOOLEAN):
          g_value_init (&result, G_TYPE_BOOLEAN);
          gboolean boolean = g_key_file_get_boolean (self->keyfile, group, key, &error);
          g_value_set_boolean (&result, boolean);
          break;
        default:
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "G_TYPE \"%s\" not handled", g_type_name (type));
          g_assert_not_reached ();
        }
    }

  if (error)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Keyfile value \"%s\" not retrieved: %s",
                        key, error->message);
      g_error_free (error);
    }

  return result;
}

static gchar *
pt_config_get_string (PtConfig *self,
                      gchar    *group,
                      gchar    *parameter)
{
  GError *error = NULL;
  gchar  *result;

  result = g_key_file_get_string (self->keyfile,
                                  group, parameter, &error);

  if (error)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Keyfile value not retrieved: %s",
                        error->message);
      g_error_free (error);
    }

  return result;
}

static gboolean
pt_config_set_string (PtConfig *self,
                      gchar    *group,
                      gchar    *parameter,
                      gchar    *value)
{
  g_key_file_set_string (
      self->keyfile, group, parameter, value);

  return pt_config_save (self);
}

/**
 * pt_config_get_id:
 * @self: a configuration instance
 *
 * The id is the filename without the extension `.asr`.
 *
 * Returns: (transfer none): the configuration’s id as a string
 *
 * Since: 5.0
 */
gchar *
pt_config_get_id (PtConfig *self)
{
  int length;

  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  if (self->id != NULL)
    return self->id;

  self->id = g_file_get_basename (self->file);
  g_assert (g_str_has_suffix (self->id, ".asr"));

  length = strlen (self->id);
  g_assert (length > 4);

  self->id[length - 4] = '\0';

  return self->id;
}

/**
 * pt_config_get_name:
 * @self: a configuration instance
 *
 * The human-visible name to identify a configuration.
 *
 * Returns: (transfer none): the configuration’s name as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_name (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return self->name;
}

/**
 * pt_config_set_name:
 * @self: a configuration instance
 * @name: the new name
 *
 * Sets the human-visible name of a configuration. It doesn’t have to be
 * unique. This saves the new name immediately to the configuration file.
 *
 * Returns: TRUE on success, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_set_name (PtConfig *self,
                    gchar    *name)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (self->is_valid, FALSE);

  if (g_strcmp0 (self->name, name) == 0)
    return TRUE;

  if (!pt_config_set_string (self, "Model", "Name", name))
    return FALSE;

  g_free (self->name);
  self->name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_NAME]);
  return TRUE;
}

/**
 * pt_config_set_base_folder:
 * @self: a configuration instance
 * @name: the new base folder
 *
 * Sets the configuration’s base folder.
 *
 * Returns: TRUE on success, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_set_base_folder (PtConfig *self,
                           gchar    *name)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (self->is_valid, FALSE);

  gboolean result;
  gboolean installed;

  result = pt_config_set_string (self, "Model", "BaseFolder", name);
  if (result)
    {
      g_free (self->base_folder);
      self->base_folder = g_strdup (name);
      installed = pt_config_verify_install (self);
      if (installed != self->is_installed)
        {
          self->is_installed = installed;
          g_object_notify_by_pspec (G_OBJECT (self),
                                    obj_properties[PROP_IS_INSTALLED]);
        }
    }

  return result;
}

/**
 * pt_config_get_base_folder:
 * @self: a configuration instance
 *
 * Gets the configuration’s base folder. If the model is not installed, the
 * base folder is not set and the return value is NULL. Another reason for
 * returning NULL is an invalid configuration, check #PtConfig:is-valid for
 * that.
 *
 * Returns: (transfer none): the configuaration’s base folder as a string or NULL
 *
 * Since: 3.0
 */
gchar *
pt_config_get_base_folder (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return self->base_folder;
}

/**
 * pt_config_get_plugin:
 * @self: a configuration instance
 *
 * Gets the name of the GStreamer plugin the configuration is intended for.
 * It has to be exactly the string GStreamer uses to instantiate the plugin.
 *
 * Returns: the plugin’s name as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_plugin (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return self->plugin;
}

/**
 * pt_config_get_downloader:
 * @self: a configuration instance
 *
 * Gets the name of the downloader plugin.
 *
 * Returns: (nullable): the plugin’s name as a string
 *
 * Since: 5.0
 */
gchar *
pt_config_get_downloader (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return self->downloader;
}

/**
 * pt_config_get_download_size:
 * @self: a configuration instance
 *
 * Gets the name of the downloader plugin.
 *
 * Returns: download size in bytes or 0 if not available
 *
 * Since: 5.0
 */
int
pt_config_get_download_size (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), 0);
  g_return_val_if_fail (self->is_valid, 0);

  GValue value = pt_config_get_value (self, "Download", "size", G_TYPE_INT);
  return g_value_get_int (&value);
}

/**
 * pt_config_get_lang_code:
 * @self: a configuration instance
 *
 * Gets the language the model was made for. It’s the ISO 639-1 code
 * (2 letters) if available, otherwise ISO 639-2 (3 letters).
 * An asterisk indicates a multilingual model.
 *
 * Returns: (transfer none): the language code as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_lang_code (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return self->lang_code;
}

/**
 * pt_config_get_lang_name:
 * @self: a configuration instance
 *
 * Gets the localized name of the language the model was made for.
 *
 * Returns: (transfer none): the language code as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_lang_name (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return self->lang_name;
}

/**
 * pt_config_get_key:
 * @self: a configuration instance
 * @key: the name of a key in the [Model] group
 *
 * Get other optional keys in the [Model] group. All keys are assumed to be
 * strings. If the key is not set, NULL is returned.
 *
 * Returns: (transfer full): the key’s value as a string or NULL
 *
 * Since: 3.0
 */
gchar *
pt_config_get_key (PtConfig *self,
                   gchar    *key)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return pt_config_get_string (self, "Model", key);
}

static gboolean
pt_config_set_property_on_object (PtConfig *self,
                                  GObject  *object,
                                  char     *config_group,
                                  char     *property_name,
                                  GError  **error)
{
  GParamSpec *spec;
  GType       type;
  GValue      value;

  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), property_name);
  if (!spec)
    {
      g_set_error (error,
                   PT_ERROR,
                   PT_ERROR_PLUGIN_MISSING_PROPERTY,
                   /* Translators: %s is replaced with the parameter’s name */
                   _ ("Plugin doesn’t have a parameter “%s”."), property_name);
      return FALSE;
    }

  if ((spec->flags & G_PARAM_WRITABLE) == 0)
    {
      g_set_error (error,
                   PT_ERROR,
                   PT_ERROR_PLUGIN_NOT_WRITABLE,
                   /* Translators: %s is replaced with the parameter’s name */
                   _ ("Parameter “%s” is not writable."), property_name);
      return FALSE;
    }

  type = G_PARAM_SPEC_VALUE_TYPE (spec);
  value = pt_config_get_value (self, config_group, property_name, type);

  if (g_param_value_validate (spec, &value))
    {
      /* TRUE == modifying value was necessary == value is not valid */
      g_set_error (error,
                   PT_ERROR,
                   PT_ERROR_PLUGIN_WRONG_VALUE,
                   /* Translators: %s is replaced with the parameter’s name */
                   _ ("Value for parameter “%s” is not valid."), property_name);
      g_value_unset (&value);
      return FALSE;
    }

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "pt-config.c: apply config key \"%s\"", property_name);
  g_object_set_property (object, property_name, &value);
  g_value_unset (&value);
  return TRUE;
}

/**
 * pt_config_configure_plugin:
 * @self: a configuration instance
 * @plugin: the ASR plugin
 * @error: (nullable): return location for an error, or NULL
 *
 * Applies a configuration to a plugin.
 *
 * Returns: TRUE on success, FALSE if a parameter could not be set
 *
 * Since: 5.0
 */
gboolean
pt_config_configure_plugin (PtConfig *self,
                            GObject  *plugin,
                            GError  **error)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (PT_IS_SPEECH_RECOGNIZER (plugin), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (self->is_valid, FALSE);

  gchar  *groups[] = { "Parameters", "Files", NULL };
  gchar **keys = NULL;

  g_object_freeze_notify (plugin);

  /* Iterate over all groups and all their keys, get the properties’
   * type and set the properties. */

  for (int g = 0; groups[g] != NULL; g++)
    {

      keys = g_key_file_get_keys (self->keyfile, groups[g],
                                  NULL, NULL);

      /* "Parameters" is optional */
      if (g == 0 && keys == NULL)
        continue;

      /* Config without "Files is not valid */
      g_assert (keys != NULL);

      for (int k = 0; keys[k] != NULL; k++)
        {
          if (!pt_config_set_property_on_object (self, plugin, groups[g], keys[k], error))
            {
              g_strfreev (keys);
              g_object_thaw_notify (plugin);
              return FALSE;
            }
        }

      g_strfreev (keys);
    }

  g_object_thaw_notify (plugin);

  return TRUE;
}

/**
 * pt_config_configure_downloader:
 * @self: a configuration instance
 * @downloader: the downloader plugin
 * @error: (nullable): return location for an error, or NULL
 *
 * Applies a configuration to a downloader plugin.
 *
 * Returns: TRUE on success, FALSE if a parameter could not be set
 *
 * Since: 5.0
 */
gboolean
pt_config_configure_downloader (PtConfig *self,
                                GObject  *downloader,
                                GError  **error)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (PT_IS_DOWNLOADER (downloader), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (self->is_valid, FALSE);

  char **keys = NULL;
  char  *id;

  g_object_freeze_notify (downloader);

  /* Config ID is taken from filename which is supposed to be unique, without extension.
   * Model data is downloaded into a folder called like the config ID. */

  id = pt_config_get_id (self);
  g_object_set (downloader, "config-id", id, NULL);

  /* Iterate over all keys in "Download" group, get the properties’
   * type and set the properties. */

  keys = g_key_file_get_keys (self->keyfile, "Download",
                              NULL, NULL);

  for (int k = 0; keys[k] != NULL; k++)
    {
      /* Omit "Plugin" key */
      if (g_strcmp0 (keys[k], "Plugin") == 0)
        continue;

      if (!pt_config_set_property_on_object (self, downloader, "Download", keys[k], error))
        {
          g_strfreev (keys);
          g_object_thaw_notify (downloader);
          return FALSE;
        }
    }

  g_strfreev (keys);

  g_object_thaw_notify (downloader);

  return TRUE;
}

void
download_finished_cb (PtDownload *download,
                      GError     *error,
                      gpointer    user_data)
{
  PtConfig *self = PT_CONFIG (user_data);
  char     *install_folder;

  /* Propagate signal to PtConfigList -> PtWindow.
   * PtWindow will present it to user itself or forward to PtModelDialog. */
  g_signal_emit (self, signals[SIGNAL_DOWNLOAD_FINISHED], 0, error);

  if (error == NULL)
    {
      install_folder = g_build_path (G_DIR_SEPARATOR_S,
                                     g_get_user_data_dir (),
                                     "parlatype",
                                     pt_config_get_id (self),
                                     NULL);

      pt_config_set_base_folder (self, install_folder);
      g_free (install_folder);
    }

  g_clear_object (&self->download);
}

/**
 * pt_config_get_download:
 * @self: self
 *
 * Gets download object or NULL.
 *
 * Returns: (transfer none): (nullable): download object
 *
 * Since: 5.0
 */
PtDownload *
pt_config_get_download (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  return self->download;
}

/**
 * pt_config_create_download:
 * @self: self
 * @error: error location
 *
 * Creates a download object.
 *
 * Returns: (transfer none): download object or NULL and an error.
 *
 * Since: 5.0
 */
PtDownload *
pt_config_create_download (PtConfig *self,
                           GError   *error)
{
  PtDownloader *downloader;

  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->download == NULL, NULL);

  self->download = pt_download_new (self->name, self->downloader);
  downloader = pt_download_get_downloader (self->download);
  pt_config_configure_downloader (self, G_OBJECT (downloader), &error);
  if (error != NULL)
    return NULL;

  g_signal_connect (self->download, "finished", G_CALLBACK (download_finished_cb), self);
  return self->download;
}

/**
 * pt_config_equal:
 * @config1: the first config to compare
 * @config2: the second config to compare
 *
 * Compares to configs for equality by comparing equality of their file representation.
 *
 * Returns: True if the two configs are equal.
 *
 * Since: 5.0
 */
gboolean
pt_config_equal (PtConfig *config1,
                 PtConfig *config2)
{
  if (config1 == NULL || config2 == NULL)
    return FALSE;

  return g_file_equal (pt_config_get_file (config1),
                       pt_config_get_file (config2));
}

static gboolean
key_is_empty (PtConfig *self,
              gchar    *key)
{
  gboolean result = FALSE;
  gchar   *value;

  value = g_key_file_get_string (self->keyfile, "Model", key, NULL);

  if (!value)
    return TRUE;
  if (g_strcmp0 (value, "") == 0)
    result = TRUE;
  g_free (value);
  return result;
}

static void
pt_config_parse_version (PtConfig *self)
{
  gchar  *version;
  gchar **version_split;
  gint64  major, minor;

  self->major_version = -1;
  self->minor_version = -1;

  version = pt_config_get_string (self, "Model", "Version");
  if (version == NULL || g_strcmp0 (version, "") == 0)
    {
      g_free (version);
      return;
    }

  version_split = g_strsplit (version, ".", 2);
  if (version_split[0] == NULL || version_split[1] == NULL)
    {
      g_free (version);
      g_strfreev (version_split);
      return;
    }

  if (g_ascii_string_to_signed (version_split[0], 10, 1, G_MAXINT, &major, NULL) &&
      g_ascii_string_to_signed (version_split[1], 10, 0, G_MAXINT, &minor, NULL))
    {
      self->major_version = (int) major;
      self->minor_version = (int) minor;
    }

  g_free (version);
  g_strfreev (version_split);
}

static gboolean
pt_config_version_is_valid (PtConfig *self)
{
  if (self->major_version == 0)
    pt_config_parse_version (self);

  return self->major_version >= 1 && self->major_version <= 2;
}

/**
 * pt_config_is_installed:
 * @self: a configuration instance
 *
 * Checks whether the model is installed, that means the base folder exists
 * and all files listed in the configuration are located inside the base folder.
 *
 * Returns: TRUE for an installed model, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_is_installed (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (self->is_valid, FALSE);

  return self->is_installed;
}

/**
 * pt_config_is_active:
 * @self: a configuration instance
 *
 * Checks whether the model is active.
 *
 * Returns: TRUE for an active config, otherwise FALSE
 *
 * Since: 5.0
 */
gboolean
pt_config_is_active (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (self->is_valid, FALSE);

  return self->active;
}

void
_pt_config_set_active (PtConfig *self,
                       gboolean  active)
{
  g_return_if_fail (PT_IS_CONFIG (self));
  g_return_if_fail (self->is_valid);

  if (self->active == active)
    return;

  self->active = active;
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_ACTIVE]);
}

/**
 * pt_config_is_downloadable:
 * @self: a configuration instance
 *
 * Checks whether the model is active.
 *
 * Returns: TRUE for an active config, otherwise FALSE
 *
 * Since: 5.0
 */
gboolean
pt_config_is_downloadable (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (self->is_valid, FALSE);

  return self->downloadable;
}

void
_pt_config_set_downloadable (PtConfig *self,
                             gboolean  downloadable)
{
  g_return_if_fail (PT_IS_CONFIG (self));
  g_return_if_fail (self->is_valid);

  if (self->downloadable == downloadable)
    return;

  self->downloadable = downloadable;
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_DOWNLOADABLE]);
}

/**
 * pt_config_is_valid:
 * @self: the configuration to test
 *
 * Checks if a configuration is formally valid:
 * <itemizedlist>
 * <listitem><para>
 * It has a version string of the form 1.x (major.minor). Major versions higher
 * than 1 are not understood by this version of libparlatype. Minor numbers are
 * supposed to be compatible and are checked only for existence
 * </para></listitem>
 * <listitem><para>
 * It has the following groups: [Model], [Files]. A [Parameters] group is optional.
 * </para></listitem>
 * <listitem><para>
 * [Model] has the following keys: Name, Plugin, BaseFolder, Language.
 * </para></listitem>
 * <listitem><para>
 * [Files] has at least one key.
 * </para></listitem>
 * <listitem><para>
 * All keys have non-empty values, except BaseFolder, which might be empty.
 * </para></listitem>
 *</itemizedlist>
 *
 * What is not tested:
 * <itemizedlist>
 * <listitem><para>
 * Optional (URL) or unknown groups or keys
 * </para></listitem>
 * <listitem><para>
 * Order of groups and keys.
 * </para></listitem>
 * <listitem><para>
 * If BaseFolder is set.
 * </para></listitem>
 * <listitem><para>
 * If the language code exists.
 * </para></listitem>
 * <listitem><para>
 * If files and paths in [Files] are formally valid (relative paths
 * with a slash as separator, e.g. subdir/subdir/file.name)
 * </para></listitem>
 * <listitem><para>
 * If files and paths exist on the file system.
 * </para></listitem>
 * <listitem><para>
 * If the plugin is installed.
 * </para></listitem>
 * <listitem><para>
 * If the plugin supports given parameters.
 * </para></listitem>
 *</itemizedlist>
 *
 * Returns: TRUE for a formally valid configuration, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_is_valid (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);

  return self->is_valid;
}

static gboolean
is_valid (PtConfig *self)
{
  char    *groups[] = { "Model", "Files", NULL };
  char    *model_keys[] = { "Name", "Plugin", "BaseFolder", "Language", NULL };
  char    *mandatory[] = { "Name", "Plugin", "Language", NULL };
  char   **parameters = NULL;
  gboolean valid = TRUE;
  int      i;

  for (i = 0; groups[i] != NULL; i++)
    {
      if (!g_key_file_has_group (self->keyfile, groups[i]))
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Config is missing group \"%s\" in %s", groups[i], self->path);
          return FALSE;
        }
    }

  if (!pt_config_version_is_valid (self))
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Config version not valid in %s", self->path);
      return FALSE;
    }

  for (i = 0; model_keys[i] != NULL; i++)
    {
      if (!g_key_file_has_key (self->keyfile, "Model", model_keys[i], NULL))
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Config is missing key \"%s\" in %s", model_keys[i], self->path);
          return FALSE;
        }
    }

  for (i = 0; mandatory[i] != NULL; i++)
    {
      if (key_is_empty (self, mandatory[i]))
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Config key \"%s\" is empty in %s", model_keys[i], self->path);
          return FALSE;
        }
    }

  parameters = g_key_file_get_keys (self->keyfile, "Files",
                                    NULL, NULL);

  if (!parameters || parameters[0] == NULL)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Config is missing parameters in group \"Files\" in %s", self->path);
      return FALSE;
    }

  g_strfreev (parameters);
  return valid;
}

static gboolean
pt_config_verify_install (PtConfig *self)
{
  gchar  **keys = NULL;
  GValue   value;
  gboolean result = TRUE;

  if (!self->base_folder)
    return FALSE;

  if (!g_file_test (self->base_folder, G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS))
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                        "MESSAGE", "Base folder \"%s\" not found for %s", self->base_folder, self->path);
      return FALSE;
    }

  keys = g_key_file_get_keys (self->keyfile, "Files",
                              NULL, NULL);

  for (int i = 0; keys[i] != NULL; i++)
    {
      value = pt_config_get_value (self, "Files", keys[i], G_TYPE_NONE);
      result = g_file_test (g_value_get_string (&value), G_FILE_TEST_EXISTS);
      if (!result)
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                          "MESSAGE", "File \"%s\" not found", g_value_get_string (&value));
      g_value_unset (&value);
      if (!result)
        break;
    }

  g_strfreev (keys);
  return result;
}

static void
free_self (PtConfig *self)
{
  if (self->keyfile)
    g_key_file_free (self->keyfile);
  if (self->file)
    g_object_unref (self->file);
  g_free (self->path);
  g_free (self->lang_name);
  g_free (self->lang_code);
  g_free (self->name);
  g_free (self->plugin);
  g_free (self->base_folder);
}

static void
tabula_rasa (PtConfig *self)
{
  self->is_valid = FALSE;
  self->is_installed = FALSE;
  self->path = NULL;
  self->file = NULL;
  self->keyfile = NULL;
  self->name = NULL;
  self->lang_name = NULL;
  self->lang_code = NULL;
  self->plugin = NULL;
  self->base_folder = NULL;
}

static void
setup_config (PtConfig *self)
{
  GError  *error = NULL;
  gboolean loaded;

  self->keyfile = g_key_file_new ();
  g_key_file_set_list_separator (self->keyfile, '/');
  self->path = g_file_get_path (self->file);
  if (!self->path)
    {
      return;
    }

  loaded = g_key_file_load_from_file (
      self->keyfile,
      self->path,
      G_KEY_FILE_KEEP_COMMENTS,
      &error);

  if (!loaded)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                        "MESSAGE", "Key file \"%s\" not loaded: %s",
                        self->path,
                        error->message);
      g_error_free (error);
    }

  if (!loaded || !is_valid (self))
    return;

  self->is_valid = TRUE;
  self->name = pt_config_get_string (self, "Model", "Name");
  self->plugin = pt_config_get_string (self, "Model", "Plugin");
  self->base_folder = pt_config_get_string (self, "Model", "BaseFolder");
  self->lang_code = pt_config_get_string (self, "Model", "Language");
  self->lang_name = gnome_get_language_from_locale (self->lang_code, NULL);
  if (self->lang_code != NULL && g_strcmp0 (self->lang_code, "*") == 0)
    self->lang_name = g_strdup (_ ("Multilingual"));
  else
    self->lang_name = gnome_get_language_from_locale (self->lang_code, NULL);
  if (!self->lang_name)
    self->lang_name = g_strdup (self->lang_code);
  self->is_installed = pt_config_verify_install (self);

  if (self->major_version == 2)
    self->downloader = pt_config_get_string (self, "Download", "Plugin");
}

/**
 * pt_config_get_file:
 * @self: a configuration instance
 *
 * The #GFile that storing the configuration.
 *
 * Returns: (transfer none): the file this object is based on
 *
 * Since: 3.0
 */
GFile *
pt_config_get_file (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);
  g_return_val_if_fail (self->is_valid, NULL);

  return self->file;
}

/**
 * pt_config_get_path:
 * @self: a configuration instance
 *
 * The path where the configuration is stored.
 *
 * Returns: (transfer none): the path or the string "(null)"
 *
 * Since: 3.0
 */
char *
pt_config_get_path (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  return self->path == NULL ? "(null)" : self->path;
}

/**
 * pt_config_set_file:
 * @self: a configuration instance
 * @file: the new file
 *
 * Sets a new file, invalidating all fields and reevaluating status.
 *
 * Since: 3.0
 */
void
pt_config_set_file (PtConfig *self,
                    GFile    *file)
{
  g_return_if_fail (PT_IS_CONFIG (self));
  g_return_if_fail (file != NULL);

  free_self (self);
  tabula_rasa (self);
  self->file = g_object_ref (file);
  setup_config (self);
}

static void
pt_config_init (PtConfig *self)
{
  tabula_rasa (self);
}

static void
pt_config_dispose (GObject *object)
{
  G_OBJECT_CLASS (pt_config_parent_class)->dispose (object);
}

static void
pt_config_finalize (GObject *object)
{
  PtConfig *self = PT_CONFIG (object);

  free_self (self);

  G_OBJECT_CLASS (pt_config_parent_class)->finalize (object);
}

static void
pt_config_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  PtConfig *self = PT_CONFIG (object);

  switch (property_id)
    {
    case PROP_FILE:
      pt_config_set_file (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_config_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  PtConfig *self = PT_CONFIG (object);

  switch (property_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;
    case PROP_IS_VALID:
      g_value_set_boolean (value, self->is_valid);
      break;
    case PROP_IS_INSTALLED:
      g_value_set_boolean (value, self->is_installed);
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;
    case PROP_DOWNLOADABLE:
      g_value_set_boolean (value, self->downloadable);
      break;
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_config_class_init (PtConfigClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = pt_config_set_property;
  G_OBJECT_CLASS (klass)->get_property = pt_config_get_property;
  G_OBJECT_CLASS (klass)->dispose = pt_config_dispose;
  G_OBJECT_CLASS (klass)->finalize = pt_config_finalize;

  /**
   * PtConfig::download-finished:
   * @self: the instance emitting the signal
   * @error: a GError or NULL
   *
   * The #PtDownload::finished signal is emitted after download success or failure.
   * In case of failure, there is en error set; in case of success the error is NULL.
   */
  signals[SIGNAL_DOWNLOAD_FINISHED] = g_signal_new ("download-finished",
                                                    PT_TYPE_CONFIG,
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    g_cclosure_marshal_VOID__BOXED,
                                                    G_TYPE_NONE,
                                                    1, G_TYPE_ERROR);

  /**
   * PtConfig:file:
   *
   * The file that was used to construct the object and contains
   * the configuration settings. This property is immutable and the file
   * can not be reloaded.
   */
  obj_properties[PROP_FILE] =
      g_param_spec_object (
          "file", NULL, NULL,
          G_TYPE_FILE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfig:is-valid:
   *
   * Indicates whether the configuration is formally valid. See
   * pt_config_is_valid() for the checks done. This property is available
   * from the very beginning and is immutable. You can not recover from
   * an invalid state.
   */
  obj_properties[PROP_IS_VALID] =
      g_param_spec_boolean (
          "is-valid", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfig:is-installed:
   *
   * Indicates whether the language model is installed.
   */
  obj_properties[PROP_IS_INSTALLED] =
      g_param_spec_boolean (
          "is-installed", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfig:active:
   *
   * Indicates whether this is the currently active config.
   */
  obj_properties[PROP_ACTIVE] =
      g_param_spec_boolean (
          "active", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfig:downloadable:
   *
   * A config is downloadable, if it’s not installed, has a plugin
   * in the [Download] section and the plugin is available.
   */
  obj_properties[PROP_DOWNLOADABLE] =
      g_param_spec_boolean (
          "downloadable", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfig:name:
   *
   * The display name for the model.
   */
  obj_properties[PROP_NAME] =
      g_param_spec_string (
          "name", NULL, NULL,
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_config_new:
 * @file: (not nullable): a file with configuration settings in .ini-like format
 *
 * Returns a new configuration instance for the given file. The configuration
 * is immediately checked for formal validity. This can be queried with
 * pt_config_is_valid() or the property #PtConfig:is-valid. The property
 * doesn’t change anymore.
 *
 * <note><para>
 * If the configuration is not valid, all methods are no-operations and
 * return FALSE or NULL.
 * </para></note>
 *
 * After use g_object_unref() it.
 *
 * Returns: (transfer full): a new #PtConfig
 *
 * Since: 3.0
 */
PtConfig *
pt_config_new (GFile *file)
{
  g_return_val_if_fail (file != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (PT_TYPE_CONFIG,
                       "file", file,
                       NULL);
}
