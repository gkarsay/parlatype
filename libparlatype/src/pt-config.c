/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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

#define GETTEXT_PACKAGE GETTEXT_LIB

#include "config.h"
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include "contrib/gnome-languages.h"
#include "pt-config.h"

typedef struct _PtConfigPrivate PtConfigPrivate;
struct _PtConfigPrivate
{
  gchar *path;
  GFile *file;
  GKeyFile *keyfile;
  gchar *name;
  gchar *lang_name;
  gchar *lang_code;
  gchar *plugin;
  gchar *base_folder;
  gboolean is_valid;
  gboolean is_installed;
};

enum
{
  PROP_FILE = 1,
  PROP_IS_VALID,
  PROP_IS_INSTALLED,
  PROP_NAME,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];
static gboolean pt_config_verify_install (PtConfig *self);

G_DEFINE_TYPE_WITH_PRIVATE (PtConfig, pt_config, G_TYPE_OBJECT)

/**
 * SECTION: pt-config
 * @short_description: Configuration for ASR plugins
 * @include: parlatype/pt-config.h
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

GQuark
pt_error_quark (void)
{
  return g_quark_from_static_string ("pt-error-quark");
}

static gboolean
pt_config_save (PtConfig *self)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  GError *error = NULL;
  gboolean result;

  result = g_key_file_save_to_file (
      priv->keyfile,
      priv->path,
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
                     gchar *group,
                     gchar *key,
                     GType type)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  GError *error = NULL;
  GValue result = G_VALUE_INIT;

  if (g_strcmp0 (group, "Files") == 0)
    {
      g_value_init (&result, G_TYPE_STRING);
      gchar *base = pt_config_get_base_folder (self);
      gchar **pieces = g_key_file_get_string_list (priv->keyfile,
                                                   "Files", key, NULL, &error);
      gchar *relative = g_build_filenamev (pieces);
      gchar *absolute = g_build_filename (base, relative, NULL);
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
          gchar *string = g_key_file_get_string (priv->keyfile,
                                                 "Parameters", key, &error);
          g_value_set_string (&result, string);
          g_free (string);
          break;
        case (G_TYPE_UINT):
        case (G_TYPE_INT):
          g_value_init (&result, G_TYPE_INT);
          gint integer = g_key_file_get_integer (priv->keyfile,
                                                 "Parameters", key, &error);
          g_value_set_int (&result, integer);
          break;
        case (G_TYPE_FLOAT):
          g_value_init (&result, G_TYPE_FLOAT);
          gfloat floaty = g_key_file_get_double (priv->keyfile,
                                                 "Parameters", key, &error);
          g_value_set_float (&result, floaty);
          break;
        case (G_TYPE_DOUBLE):
          g_value_init (&result, G_TYPE_DOUBLE);
          gdouble doubly = g_key_file_get_double (priv->keyfile,
                                                  "Parameters", key, &error);
          g_value_set_double (&result, doubly);
          break;
        case (G_TYPE_BOOLEAN):
          g_value_init (&result, G_TYPE_BOOLEAN);
          gboolean boolean = g_key_file_get_boolean (priv->keyfile,
                                                     "Parameters", key, &error);
          g_value_set_boolean (&result, boolean);
          break;
        default:
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                            "MESSAGE", "G_TYPE \"%s\" not handled", g_type_name (type));
          g_assert_not_reached ();
        }
    }

  if (error)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Keyfile value \"%s\" not retrieved: %s",
                        key, error->message);
      g_error_free (error);
    }

  return result;
}

static gchar *
pt_config_get_string (PtConfig *self,
                      gchar *group,
                      gchar *parameter)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  GError *error = NULL;
  gchar *result;

  result = g_key_file_get_string (priv->keyfile,
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
                      gchar *group,
                      gchar *parameter,
                      gchar *value)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_key_file_set_string (
      priv->keyfile, group, parameter, value);

  return pt_config_save (self);
}

/**
 * pt_config_get_name:
 * @self: a configuration instance
 *
 * The human-visible name to identify a configuration.
 *
 * Return value: (transfer none): the configuration’s name as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_name (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, NULL);

  return priv->name;
}

/**
 * pt_config_set_name:
 * @self: a configuration instance
 * @name: the new name
 *
 * Sets the human-visible name of a configuration. It doesn’t have to be
 * unique. This saves the new name immediately to the configuration file.
 *
 * Return value: TRUE on success, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_set_name (PtConfig *self,
                    gchar *name)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, FALSE);

  if (g_strcmp0 (priv->name, name) == 0)
    return TRUE;

  if (!pt_config_set_string (self, "Model", "Name", name))
    return FALSE;

  g_free (priv->name);
  priv->name = g_strdup (name);
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
 * Return value: TRUE on success, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_set_base_folder (PtConfig *self,
                           gchar *name)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, FALSE);

  gboolean result;
  gboolean installed;

  result = pt_config_set_string (self, "Model", "BaseFolder", name);
  if (result)
    {
      g_free (priv->base_folder);
      priv->base_folder = g_strdup (name);
      installed = pt_config_verify_install (self);
      if (installed != priv->is_installed)
        {
          priv->is_installed = installed;
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
 * Return value: (transfer none): the configuaration’s base folder as a string or NULL
 *
 * Since: 3.0
 */
gchar *
pt_config_get_base_folder (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, NULL);

  return priv->base_folder;
}

/**
 * pt_config_get_plugin:
 * @self: a configuration instance
 *
 * Gets the name of the GStreamer plugin the configuration is intended for.
 * It has to be exactly the string GStreamer uses to instantiate the plugin.
 *
 * Return value: the plugin’s name as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_plugin (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, NULL);

  return priv->plugin;
}

/**
 * pt_config_get_lang_code:
 * @self: a configuration instance
 *
 * Gets the language the model was made for. It’s the ISO 639-1 code
 * (2 letters) if available, otherwise ISO 639-2 (3 letters).
 *
 * Return value: (transfer none): the language code as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_lang_code (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, NULL);

  return priv->lang_code;
}

/**
 * pt_config_get_lang_name:
 * @self: a configuration instance
 *
 * Gets the localized name of the language the model was made for.
 *
 * Return value: (transfer none): the language code as a string
 *
 * Since: 3.0
 */
gchar *
pt_config_get_lang_name (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, NULL);

  return priv->lang_name;
}

/**
 * pt_config_get_key:
 * @self: a configuration instance
 * @key: the name of a key in the [Model] group
 *
 * Get other optional keys in the [Model] group. All keys are assumed to be
 * strings. If the key is not set, NULL is returned.
 *
 * Return value: (transfer full): the key’s value as a string or NULL
 *
 * Since: 3.0
 */
gchar *
pt_config_get_key (PtConfig *self,
                   gchar *key)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, NULL);

  return pt_config_get_string (self, "Model", key);
}

/**
 * pt_config_apply:
 * @self: a configuration instance
 * @plugin: the GStreamer ASR plugin
 * @error: (nullable): return location for an error, or NULL
 *
 * Applies a configuration to a GStreamer plugin.
 *
 * Return value: TRUE on success, FALSE if a parameter could not be set
 *
 * Since: 3.0
 */
gboolean
pt_config_apply (PtConfig *self,
                 GObject *plugin,
                 GError **error)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (plugin), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, FALSE);

  gchar **keys = NULL;
  GParamSpec *spec;
  GType type;
  GValue value;
  gchar *groups[] = { "Parameters", "Files", NULL };

  g_object_freeze_notify (plugin);

  /* Iterate over all groups and all their keys, get the properties’
   * type and set the properties. */

  for (int g = 0; groups[g] != NULL; g++)
    {

      keys = g_key_file_get_keys (priv->keyfile, groups[g],
                                  NULL, NULL);

      /* "Parameters" is optional */
      if (!keys)
        continue;

      for (int k = 0; keys[k] != NULL; k++)
        {
          spec = g_object_class_find_property (G_OBJECT_GET_CLASS (plugin), keys[k]);
          if (!spec)
            {
              g_set_error (error,
                           PT_ERROR,
                           PT_ERROR_PLUGIN_MISSING_PROPERTY,
                           _ ("Plugin doesn’t have a property “%s”"), keys[k]);
              g_strfreev (keys);
              g_object_thaw_notify (plugin);
              return FALSE;
            }
          if ((spec->flags & G_PARAM_WRITABLE) == 0)
            {
              g_set_error (error,
                           PT_ERROR,
                           PT_ERROR_PLUGIN_NOT_WRITABLE,
                           _ ("Plugin property “%s” is not writable"), keys[k]);
              g_strfreev (keys);
              g_object_thaw_notify (plugin);
              return FALSE;
            }

          type = G_PARAM_SPEC_VALUE_TYPE (spec);
          value = pt_config_get_value (self, groups[g], keys[k], type);

          if (g_param_value_validate (spec, &value))
            {
              /* TRUE == modifying value was necessary == value is not valid */
              g_set_error (error,
                           PT_ERROR,
                           PT_ERROR_PLUGIN_WRONG_VALUE,
                           _ ("Value for property “%s” is not valid"), keys[k]);
              g_strfreev (keys);
              g_value_unset (&value);
              g_object_thaw_notify (plugin);
              return FALSE;
            }

          g_object_set_property (plugin, keys[k], &value);
          g_value_unset (&value);
        }

      g_strfreev (keys);
    }

  g_object_thaw_notify (plugin);

  return TRUE;
}

static gboolean
key_is_empty (PtConfig *self,
              gchar *key)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  gboolean result = FALSE;
  gchar *value;

  value = g_key_file_get_string (priv->keyfile, "Model", key, NULL);

  if (!value)
    return TRUE;
  if (g_strcmp0 (value, "") == 0)
    result = TRUE;
  g_free (value);
  return result;
}

static gboolean
pt_config_version_is_valid (PtConfig *self)
{
  gchar *version;
  gchar **version_split;
  gint64 major;
  gboolean success;

  version = pt_config_get_string (self, "Model", "Version");
  if (!version || g_strcmp0 (version, "") == 0)
    {
      g_free (version);
      return FALSE;
    }

  version_split = g_strsplit (version, ".", 2);
  success = g_ascii_string_to_signed (version_split[0],
                                      10, 1, 1, /* base, min, max */
                                      &major, NULL);
  g_free (version);
  if (!success)
    {
      g_strfreev (version_split);
      return FALSE;
    }

  if (!version_split[1])
    {
      g_strfreev (version_split);
      return FALSE;
    }

  success = g_ascii_string_to_signed (version_split[1],
                                      10, 0, G_MAXINT,
                                      NULL, NULL);
  g_strfreev (version_split);
  return success;
}

/**
 * pt_config_is_installed:
 * @self: a configuration instance
 *
 * Checks whether the model is installed, that means the base folder exists
 * and all files listed in the configuration are located inside the base folder.
 *
 * Return value: TRUE for an installed model, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_is_installed (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, FALSE);

  return priv->is_installed;
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
 * Return value: TRUE for a formally valid configuration, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_config_is_valid (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), FALSE);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  return priv->is_valid;
}

static gboolean
is_valid (PtConfig *self)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  gchar *groups[] = { "Model", "Files", NULL };
  gchar *model_keys[] = { "Name", "Plugin", "BaseFolder", "Language", NULL };
  gchar *mandatory[] = { "Name", "Plugin", "Language", NULL };
  gchar **parameters = NULL;
  gboolean valid = TRUE;
  int i;

  for (i = 0; groups[i] != NULL; i++)
    {
      if (!g_key_file_has_group (priv->keyfile, groups[i]))
        return FALSE;
    }

  if (!pt_config_version_is_valid (self))
    return FALSE;

  for (i = 0; model_keys[i] != NULL; i++)
    {
      if (!g_key_file_has_key (priv->keyfile, "Model", model_keys[i], NULL))
        return FALSE;
    }

  for (i = 0; mandatory[i] != NULL; i++)
    {
      if (key_is_empty (self, mandatory[i]))
        return FALSE;
    }

  parameters = g_key_file_get_keys (priv->keyfile, "Files",
                                    NULL, NULL);

  if (!parameters || parameters[0] == NULL)
    return FALSE;

  g_strfreev (parameters);
  return valid;
}

static gboolean
pt_config_verify_install (PtConfig *self)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  gchar **keys = NULL;
  GValue value;
  gboolean result = TRUE;

  if (!priv->base_folder)
    return FALSE;

  if (!g_file_test (priv->base_folder, G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS))
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                        "MESSAGE", "Base folder \"%s\" not found", priv->base_folder);
      return FALSE;
    }

  keys = g_key_file_get_keys (priv->keyfile, "Files",
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
free_private (PtConfigPrivate *priv)
{
  if (priv->keyfile)
    g_key_file_free (priv->keyfile);
  if (priv->file)
    g_object_unref (priv->file);
  g_free (priv->path);
  g_free (priv->lang_name);
  g_free (priv->lang_code);
  g_free (priv->name);
  g_free (priv->plugin);
  g_free (priv->base_folder);
}

static void
tabula_rasa (PtConfigPrivate *priv)
{
  priv->is_valid = FALSE;
  priv->is_installed = FALSE;
  priv->path = NULL;
  priv->file = NULL;
  priv->keyfile = NULL;
  priv->name = NULL;
  priv->lang_name = NULL;
  priv->lang_code = NULL;
  priv->plugin = NULL;
  priv->base_folder = NULL;
}

static void
setup_config (PtConfig *self)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  GError *error = NULL;
  gboolean loaded;

  priv->keyfile = g_key_file_new ();
  g_key_file_set_list_separator (priv->keyfile, '/');
  priv->path = g_file_get_path (priv->file);
  if (!priv->path)
    {
      return;
    }

  loaded = g_key_file_load_from_file (
      priv->keyfile,
      priv->path,
      G_KEY_FILE_KEEP_COMMENTS,
      &error);

  if (!loaded)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                        "MESSAGE", "Key file \"%s\" not loaded: %s",
                        priv->path,
                        error->message);
      g_error_free (error);
    }

  if (!loaded || !is_valid (self))
    return;

  priv->is_valid = TRUE;
  priv->name = pt_config_get_string (self, "Model", "Name");
  priv->plugin = pt_config_get_string (self, "Model", "Plugin");
  priv->base_folder = pt_config_get_string (self, "Model", "BaseFolder");
  priv->lang_code = pt_config_get_string (self, "Model", "Language");
  priv->lang_name = gnome_get_language_from_locale (priv->lang_code, NULL);
  if (!priv->lang_name)
    priv->lang_name = g_strdup (priv->lang_code);
  priv->is_installed = pt_config_verify_install (self);
}

/**
 * pt_config_get_file:
 * @self: a configuration instance
 *
 * The #GFile that storing the configuration.
 *
 * Return value: (transfer none): the file this object is based on
 *
 * Since: 3.0
 */
GFile *
pt_config_get_file (PtConfig *self)
{
  g_return_val_if_fail (PT_IS_CONFIG (self), NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);
  g_return_val_if_fail (priv->is_valid, NULL);

  return priv->file;
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
                    GFile *file)
{
  g_return_if_fail (PT_IS_CONFIG (self));
  g_return_if_fail (file != NULL);

  PtConfigPrivate *priv = pt_config_get_instance_private (self);

  free_private (priv);
  tabula_rasa (priv);
  priv->file = g_object_ref (file);
  setup_config (self);
}

static void
pt_config_init (PtConfig *self)
{
  PtConfigPrivate *priv = pt_config_get_instance_private (self);

  tabula_rasa (priv);
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
  PtConfigPrivate *priv = pt_config_get_instance_private (self);

  free_private (priv);

  G_OBJECT_CLASS (pt_config_parent_class)->finalize (object);
}

static void
pt_config_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
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
pt_config_get_property (GObject *object,
                        guint property_id,
                        GValue *value,
                        GParamSpec *pspec)
{
  PtConfig *self = PT_CONFIG (object);
  PtConfigPrivate *priv = pt_config_get_instance_private (self);

  switch (property_id)
    {
    case PROP_FILE:
      g_value_set_object (value, priv->file);
      break;
    case PROP_IS_VALID:
      g_value_set_boolean (value, priv->is_valid);
      break;
    case PROP_IS_INSTALLED:
      g_value_set_boolean (value, priv->is_installed);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
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
 * Return value: (transfer full): a new #PtConfig
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
