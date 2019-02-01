/* Copyright (C) Gabor Karsay 2018 <gabor.karsay@gmx.at>
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
#include <gio/gio.h>
#define GETTEXT_PACKAGE "libparlatype"
#include <glib/gi18n-lib.h>
#include "pt-asr-settings.h"

struct _PtAsrSettingsPrivate
{
	gchar    *filename;
	GKeyFile *keyfile;
};

enum
{
	PROP_0,
	PROP_KEYFILE,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };


G_DEFINE_TYPE_WITH_PRIVATE (PtAsrSettings, pt_asr_settings, G_TYPE_OBJECT)


/**
 * SECTION: pt-asr-settings
 * @short_description: 
 * @stability: Unstable
 * @include: parlatype/pt-asr-settings.h
 *
 * TODO
 */


static gboolean
pt_asr_settings_save_keyfile (PtAsrSettings *settings)
{
	GError   *error = NULL;
	gboolean  result;

	result = g_key_file_save_to_file (
			settings->priv->keyfile,
			settings->priv->filename,
			&error);

	if (error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			          "MESSAGE", "Keyfile not saved: %s", error->message);
		g_error_free (error);
	}		

	return result;
}


/**
 * pt_asr_settings_set_int:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config
 * @field: the field in the config
 * @value: the new integer value
 *
 * Sets a new integer value. If the config doesn't exist, it will be created.
 * Likewise the field will be created, if it doesn't exist yet.
 * 
 * Return value: TRUE on success, FALSE on failure
 */
gboolean
pt_asr_settings_set_int (PtAsrSettings *settings,
                         gchar *id,
                         gchar *field,
                         gint   value)
{
	g_key_file_set_integer (
			settings->priv->keyfile,
			id, field, value);

	return pt_asr_settings_save_keyfile (settings);
}


/**
 * pt_asr_settings_get_int:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config
 * @field: the field asked for
 *
 * Gets an integer value from a field for a certain config. If it's not found,
 * the return value is 0.
 * 
 * Return value: the integer value or 0 in case of failure
 */
gint
pt_asr_settings_get_int (PtAsrSettings *settings,
                         gchar *id,
                         gchar *field)
{
	GError *error = NULL;
	gint    result;

	result = g_key_file_get_integer (
			settings->priv->keyfile,
			id, field, &error);
	
	if (error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			          "MESSAGE", "Keyfile value not retrieved: %s", error->message);
		g_error_free (error);
	}		

	return result;
}

/**
 * pt_asr_settings_set_boolean:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config
 * @field: the field in the config
 * @value: the new value
 *
 * Sets a boolean value. If the config doesn't exist, it will be created.
 * Likewise the field will be created, if it doesn't exist yet.
 * 
 * Return value: TRUE on success, FALSE on failure
 */
gboolean
pt_asr_settings_set_boolean (PtAsrSettings *settings,
                             gchar *id,
                             gchar *field,
                             gboolean value)
{
	g_key_file_set_boolean (
			settings->priv->keyfile,
			id, field, value);

	return pt_asr_settings_save_keyfile (settings);
}

/**
 * pt_asr_settings_get_boolean:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config
 * @field: the field asked for
 *
 * Gets a boolean value from a field for a certain config. If it's not found,
 * the return value is FALSE.
 * 
 * Return value: the boolean value or FALSE if it wasn't found
 */
gboolean
pt_asr_settings_get_boolean (PtAsrSettings *settings,
                             gchar *id,
                             gchar *field)
{
	GError   *error = NULL;
	gboolean  result;

	result = g_key_file_get_boolean (
			settings->priv->keyfile,
			id, field, &error);
	
	if (error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			          "MESSAGE", "Keyfile value not retrieved: %s", error->message);
		g_error_free (error);
	}		

	return result;
}

/**
 * pt_asr_settings_set_string:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config
 * @field: the field in the config
 * @string: the new string
 *
 * Sets a new string. If the config doesn't exist, it will be created.
 * Likewise the field will be created, if it doesn't exist yet.
 * 
 * Return value: TRUE on success, FALSE on failure
 */
gboolean
pt_asr_settings_set_string (PtAsrSettings *settings,
                            gchar *id,
                            gchar *field,
                            gchar *string)
{
	g_key_file_set_string (
			settings->priv->keyfile,
			id, field, string);

	return pt_asr_settings_save_keyfile (settings);
}


/**
 * pt_asr_settings_get_string:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config
 * @field: the field asked for
 *
 * Gets a string from a field for a certain config. If it's not found,
 * the return value is NULL.
 * 
 * Return value: a newly allocated string or NULL if it wasn't found
 */
gchar*
pt_asr_settings_get_string (PtAsrSettings *settings,
                            gchar *id,
                            gchar *field)
{
	GError *error = NULL;
	gchar  *result;

	result = g_key_file_get_string (
			settings->priv->keyfile,
			id, field, &error);
	
	if (error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			          "MESSAGE", "Keyfile value not retrieved: %s", error->message);
		g_error_free (error);
	}		

	return result;
}

                        
/**
 * pt_asr_settings_get_configs:
 * @settings: a #PtAsrSettings instance
 *
 * Returns the names (IDs) of all saved configurations or NULL, if there are none.
 * 
 * After use g_object_unref() it.
 *
 * Return value: (array zero-terminated=1)(transfer full): a newly-allocated
 * NULL-terminated array of strings. Use g_strfreev() to free it.
 */
gchar**
pt_asr_settings_get_configs (PtAsrSettings *settings)
{
	return g_key_file_get_groups (settings->priv->keyfile, NULL);
}

/**
 * pt_asr_settings_have_configs:
 * @settings: a #PtAsrSettings instance
 *
 * Returns TRUE if there are any configurations stored.
 *
 * Return value: TRUE for existing configurations, otherwise FALSE
 */
gboolean
pt_asr_settings_have_configs (PtAsrSettings *settings)
{
	gboolean   result = FALSE;
	gchar    **groups;

	groups = g_key_file_get_groups (settings->priv->keyfile, NULL);
	if (groups[0])
		result = TRUE;
	g_strfreev (groups);

	return result;
}


/**
 * pt_asr_settings_add_config:
 * @settings: a #PtAsrSettings instance
 * @name: the descriptive name of the new config, shown to the user
 * @type: the ASR engine used
 *
 * Adds a new configuration, with a descriptive name and a type. The name
 * should be but doesn't have to be unique. The only type allowed is "sphinx".
 * 
 * Return value: (transfer full): the ID of the new config as a string,
 * or NULL in case of failure
 */
gchar*
pt_asr_settings_add_config (PtAsrSettings *settings,
                            gchar *name,
                            gchar *type)
{
	gchar *group;
	gint id = 1;
	gboolean unique = FALSE;

	if (g_strcmp0 (type, "sphinx") != 0)
		return NULL;

	while (!unique) {
		group = g_strdup_printf ("config%d", id);
		if (g_key_file_has_group (settings->priv->keyfile, group)) {
			g_free (group);
			id++;
		} else {
			unique = TRUE;
		}
	}

	g_key_file_set_string (
			settings->priv->keyfile,
			group, "name", name);

	g_key_file_set_string (
			settings->priv->keyfile,
			group, "type", type);

	if (!pt_asr_settings_save_keyfile (settings)) {
		g_key_file_remove_group (settings->priv->keyfile, group, NULL);
		g_free (group);
		group = NULL;
	}
	
	return group;	
}

/**
 * pt_asr_settings_remove_config:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config to remove
 *
 * Removes a complete configuration. On success it's gone forever.
 * 
 * Return value: TRUE on success, FALSE on failure
 */
gboolean
pt_asr_settings_remove_config (PtAsrSettings *settings,
                               gchar *id)
{
	GError *error = NULL;
	gboolean result;

	result = g_key_file_remove_group (settings->priv->keyfile, id, &error);
	if (error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			          "MESSAGE", "Removing group failed: %s", error->message);
		g_error_free (error);
		return result;
	}		

	return pt_asr_settings_save_keyfile (settings);
}

gboolean
apply_sphinx (PtAsrSettings *settings,
              gchar         *id,
              GObject       *sphinx)
{
	gchar *lm, *dict, *hmm;

	lm = pt_asr_settings_get_string (settings, id, "lm");
	dict = pt_asr_settings_get_string (settings, id, "dict");
	hmm = pt_asr_settings_get_string (settings, id, "hmm");

	g_object_set (sphinx,
			"lm", lm,
			"dict", dict,
			"hmm", hmm,
			NULL);

	g_free (lm);
	g_free (dict);
	g_free (hmm);

	return TRUE;
}

/**
 * pt_asr_settings_apply_config:
 * @settings: a #PtAsrSettings instance
 * @id: the ID of the config to apply
 * @player: the PtPlayer to setup
 *
 * Applies a configuration to a PtPlayer.
 * 
 * Return value: TRUE on success, FALSE on failure
 */
gboolean
pt_asr_settings_apply_config (PtAsrSettings *settings,
                              gchar         *id,
                              PtPlayer      *player)
{
	gchar *type;

	type = pt_asr_settings_get_string (settings, id, "type");
	if (g_strcmp0 (type, "sphinx") == 0) {
		g_free (type);
		return apply_sphinx (settings, id, player->sphinx);
	}
	g_free (type);
	return FALSE;
}

static void
pt_asr_settings_constructed (GObject *object)
{
	PtAsrSettings *settings = PT_ASR_SETTINGS (object);
	GError *error = NULL;

	settings->priv->keyfile = g_key_file_new ();
	g_key_file_load_from_file (
			settings->priv->keyfile,
		        settings->priv->filename,
		        G_KEY_FILE_KEEP_COMMENTS,
		        &error);

	if (error) {
		if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			g_key_file_load_from_file (
					settings->priv->keyfile,
					"resource:///com/github/gkarsay/libparlatype/asr.ini",
					G_KEY_FILE_KEEP_COMMENTS,
					NULL);
		} else {
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
					  "MESSAGE", "Key file not loaded: %s", error->message);
		}
		g_error_free (error);
	}		
}

static void
pt_asr_settings_init (PtAsrSettings *settings)
{
	settings->priv = pt_asr_settings_get_instance_private (settings);
}

static void
pt_asr_settings_dispose (GObject *object)
{
	G_OBJECT_CLASS (pt_asr_settings_parent_class)->dispose (object);
}

static void
pt_asr_settings_finalize (GObject *object)
{
	PtAsrSettings *settings = PT_ASR_SETTINGS (object);

	g_free (settings->priv->filename);
	g_key_file_free (settings->priv->keyfile);

	G_OBJECT_CLASS (pt_asr_settings_parent_class)->finalize (object);
}

static void
pt_asr_settings_set_property (GObject      *object,
			      guint         property_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	PtAsrSettings *settings = PT_ASR_SETTINGS (object);

	switch (property_id) {
	case PROP_KEYFILE:
		g_free (settings->priv->filename);
		settings->priv->filename = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_asr_settings_get_property (GObject    *object,
			      guint       property_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	PtAsrSettings *settings = PT_ASR_SETTINGS (object);

	switch (property_id) {
	case PROP_KEYFILE:
		g_value_set_string (value, settings->priv->filename);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_asr_settings_class_init (PtAsrSettingsClass *klass)
{
	G_OBJECT_CLASS (klass)->set_property = pt_asr_settings_set_property;
	G_OBJECT_CLASS (klass)->get_property = pt_asr_settings_get_property;
	G_OBJECT_CLASS (klass)->constructed  = pt_asr_settings_constructed;
	G_OBJECT_CLASS (klass)->dispose = pt_asr_settings_dispose;
	G_OBJECT_CLASS (klass)->finalize = pt_asr_settings_finalize;

	/**
	* PtAsrSettings:keyfile:
	*
	* The keyfile to use.
	*/
	obj_properties[PROP_KEYFILE] =
	g_param_spec_string (
			"keyfile",
			"Key file",
			"Name of the key file",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

/**
 * pt_asr_settings_new:
 * @settings_file: path to the file with the settings
 *
 * Returns a new PtAsrSettings instance for the given file. If the file
 * doesn't exist, it will be created.
 *
 * After use g_object_unref() it.
 *
 * Return value: (transfer full): a new #PtAsrSettings
 */
PtAsrSettings *
pt_asr_settings_new (gchar *settings_file)
{
	return g_object_new (PT_TYPE_ASR_SETTINGS,
			"keyfile", settings_file,
			NULL);
}
