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

/**
 * PtAsrManager:
 *
 * Manages things.
 *
 * Still in development.
 *
 * Since: 5.0
 */

#define G_LOG_DOMAIN "pt-asr-manager"

#include "config.h"

#include "pt-asr-manager.h"

#include "pt-speech-recognizer.h"
#include "pt-window.h"

#include <gio/gio.h>
#include <libpeas.h>

struct _PtAsrManager
{
  GObject parent_instance;

  GSettings          *settings;
  PtWindow           *window;
  GtkWidget          *asr_controls;
  PtPlayer           *player;
  PtTextView         *text_view;
  PeasEngine         *engine;
  PeasPluginInfo     *plugin_info;
  PtSpeechRecognizer *plugin;
  PeasExtensionSet   *extension_set;
  PtConfig           *config;
  gboolean            have_asr;
};

enum
{
  PROP_0,
  PROP_PLAYER,
  PROP_TEXT_VIEW,
  PROP_ASR_CONTROLS,
  PROP_HAVE_ASR,
  PROP_HAVE_CONFIG,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_TYPE (PtAsrManager, pt_asr_manager, G_TYPE_OBJECT)

static void
on_extension_removed (PeasExtensionSet   *set,
                      PeasPluginInfo     *info,
                      PtSpeechRecognizer *plugin)
{
  pt_speech_recognizer_unload (plugin);
}

static void
on_extension_added (PeasExtensionSet   *set,
                    PeasPluginInfo     *info,
                    PtSpeechRecognizer *plugin,
                    gpointer            user_data)
{
  PtAsrManager *self = PT_ASR_MANAGER (user_data);
  GError       *error = NULL;
  GTask        *task;

  g_assert (PT_IS_PLAYER (self->player));
  g_assert (PT_IS_TEXT_VIEW (self->text_view));

  task = g_object_get_data (G_OBJECT (info), "TASK");

  self->plugin_info = info;
  self->plugin = plugin;
  if (!pt_config_apply (self->config, G_OBJECT (plugin), &error))
    {
      g_task_return_error (task, error);
      return;
    }
  pt_speech_recognizer_load (plugin, self->player, self->text_view, self->asr_controls);

  g_task_return_boolean (task, TRUE);
}

void
pt_asr_manager_stop_transcribing (PtAsrManager *self)
{
  g_return_if_fail (PT_IS_ASR_MANAGER (self));
  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self->plugin));

  pt_player_pause (self->player);
  pt_player_set_mode (self->player, PT_MODE_PLAYBACK);
  pt_speech_recognizer_stop (PT_SPEECH_RECOGNIZER (self->plugin));
}

void
pt_asr_manager_start_transcribing (PtAsrManager *self)
{
  g_return_if_fail (PT_IS_ASR_MANAGER (self));
  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self->plugin));

  if (pt_player_set_mode (self->player, PT_MODE_ASR))
    {
      pt_player_play (self->player);
      pt_speech_recognizer_start (PT_SPEECH_RECOGNIZER (self->plugin));
    }
}

/**
 * pt_asr_manager_activate_finish:
 * @self: a `PtAsrManager`
 * @result: a `GAsyncResult`
 * @error:
 *
 * Finishes the [method@Pt.AsrManager.activate_async] call.
 * Returns: True on success.
 */
gboolean
pt_asr_manager_load_plugin_finish (PtAsrManager *self,
                                   GAsyncResult *result,
                                   GError      **error)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
pt_asr_manager_unload_plugin (PtAsrManager *self)
{
  if (self->plugin_info == NULL)
    {
      return;
    }

  peas_engine_unload_plugin (self->engine, self->plugin_info);
  self->plugin_info = NULL;
}

/**
 * pt_asr_manager_activate_async:
 * @self: a `PtAsrManager`
 * @config: a `PtConfig`
 * @cancellable: A `GCancellable` to cancel the operation.
 * @callback: A callback to call when the operation is complete.
 * @user_data: Data to pass to `callback`.
 *
 * Loads the plugin referenced in PtConfig, applies all properties of PtConfig
 * to the plugin and makes it ready for action.
 *
 * Returns early, if the plugin is already loaded.
 *
 * Steps of loading process:
 *
 * * peas_engine_load_plugin() instantiates plugin
 * * continued in callback on_extension_added()
 * * PtConfig is applied to plugin
 * * plugin's load method is called (expected to finished on return)
 */
void
pt_asr_manager_load_plugin_async (PtAsrManager       *self,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data)
{
  g_return_if_fail (PT_IS_ASR_MANAGER (self));
  g_return_if_fail (PT_IS_CONFIG (self->config));
  g_return_if_fail (self->engine != NULL);
  g_return_if_fail (self->player != NULL);
  g_return_if_fail (self->text_view != NULL);

  GTask          *task;
  char           *plugin_name;
  PeasPluginInfo *plugin_info;

  task = g_task_new (self, cancellable, callback, user_data);
  plugin_name = pt_config_get_plugin (self->config);
  plugin_info = peas_engine_get_plugin_info (self->engine, plugin_name);

  if (plugin_info == NULL)
    {
      g_task_return_new_error (task, PT_ERROR,
                               PT_ERROR_PLUGIN_NOT_FOUND,
                               "Plugin %s not found", plugin_name);
      g_object_unref (task);
      return;
    }

  /* Unload previous plugin first */
  if (self->plugin_info != plugin_info)
    {
      pt_asr_manager_unload_plugin (self);
    }

  /* A plugin is already loaded */
  if (self->plugin_info != NULL)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      return;
    }

  g_object_set_data_full (G_OBJECT (plugin_info),
                          "TASK",
                          g_steal_pointer (&task),
                          g_object_unref);
  peas_engine_load_plugin (self->engine, plugin_info);
}

/**
 * pt_asr_manager_plugin_is_available:
 * @self: a `PtAsrManager`
 * @config: a PtConfig
 *
 * Reads the plugin name of the config and checks whether this plugin is available
 * on the system.
 *
 * Returns: TRUE for an available plugin.
 */
gboolean
pt_asr_manager_plugin_is_available (PtAsrManager *self,
                                    PtConfig     *config)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  g_return_val_if_fail (PT_IS_CONFIG (config), FALSE);
  g_return_val_if_fail (self->engine != NULL, FALSE);

  char    *plugin_name;
  gboolean success;

  plugin_name = pt_config_get_plugin (config);
  if (plugin_name == NULL)
    return FALSE;

  PeasPluginInfo *info = peas_engine_get_plugin_info (self->engine, plugin_name);
  success = info != NULL;

  if (!success)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Plugin %s not available for %s", plugin_name, pt_config_get_path (config));
    }

  return success;
}

PtConfig *
pt_asr_manager_get_config (PtAsrManager *self)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), NULL);

  return self->config;
}

gboolean
pt_asr_manager_set_config (PtAsrManager *self,
                           PtConfig     *config)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  g_return_val_if_fail (PT_IS_CONFIG (config) || config == NULL, FALSE);

  GFile   *config_file;
  char    *path;
  gboolean success;

  if (self->config == NULL && config == NULL)
    {
      return TRUE;
    }

  if (config == NULL)
    {
      success = g_settings_set_string (self->settings, "asr-config", "");
      if (success)
        {
          g_clear_object (&self->config);
          pt_asr_manager_unload_plugin (self);
        }
    }
  else
    {
      if (!pt_config_is_installed (config))
        {
          return FALSE;
        }
      config_file = pt_config_get_file (config);
      path = g_file_get_path (config_file);
      success = g_settings_set_string (self->settings, "asr-config", path);
      if (success)
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "asr-config set to %s", path);
          g_clear_object (&self->config);
          self->config = g_object_ref (config);
          pt_asr_manager_load_plugin_async (self, NULL, NULL, self);
        }
      g_free (path);
      g_object_unref (config_file);
    }

  return success;
}

/**
 * pt_asr_manager_have_asr:
 * @self: a `PtAsrManager`
 *
 * See #PtAsrManager::have-asr.
 * on the system.
 *
 * Returns: True if ASR is available.
 */
gboolean
pt_asr_manager_have_asr (PtAsrManager *self)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  return self->have_asr;
}

/**
 * pt_asr_manager_have_config:
 * @self: a `PtAsrManager`
 *
 * See #PtAsrManager::have-config.
 *
 * Returns: True if a usable config is available.
 */
gboolean
pt_asr_manager_have_config (PtAsrManager *self)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  return self->config != NULL;
}

static void
pt_asr_manager_load_config (PtAsrManager *self)
{
  GFile    *asr_file;
  gchar    *asr_path;
  PtConfig *config;

  asr_path = g_settings_get_string (self->settings, "asr-config");
  asr_file = g_file_new_for_path (asr_path);
  config = pt_config_new (asr_file);
  g_object_unref (asr_file);

  if (!pt_config_is_valid (config) ||
      !pt_asr_manager_plugin_is_available (self, config) ||
      !pt_config_is_installed (config))
    {
      g_object_unref (config);
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "config not usable: »%s«", asr_path);
    }
  else
    {
      self->config = config;
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "config found: »%s«", asr_path);
    }

  g_free (asr_path);
}

static void
pt_asr_manager_dispose (GObject *object)
{
  PtAsrManager *self = PT_ASR_MANAGER (object);

  if (self->engine != NULL)
    {
      if (self->plugin_info != NULL)
        {
          peas_engine_unload_plugin (self->engine, self->plugin_info);
        }
      peas_engine_garbage_collect (self->engine);
    }
  g_clear_object (&self->extension_set);
  g_clear_object (&self->plugin_info);
  g_clear_object (&self->config);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (pt_asr_manager_parent_class)->dispose (object);
}

static void
pt_asr_manager_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PtAsrManager *self = PT_ASR_MANAGER (object);

  switch (property_id)
    {
    case PROP_PLAYER:
      self->player = g_value_get_object (value);
      break;
    case PROP_TEXT_VIEW:
      self->text_view = g_value_get_object (value);
      break;
    case PROP_ASR_CONTROLS:
      self->asr_controls = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_asr_manager_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PtAsrManager *self = PT_ASR_MANAGER (object);

  switch (property_id)
    {
    case PROP_PLAYER:
      g_value_set_object (value, self->player);
      break;
    case PROP_TEXT_VIEW:
      g_value_set_object (value, self->text_view);
      break;
    case PROP_ASR_CONTROLS:
      g_value_set_object (value, self->asr_controls);
      break;
    case PROP_HAVE_ASR:
      g_value_set_boolean (value, self->have_asr);
      break;
    case PROP_HAVE_CONFIG:
      g_value_set_boolean (value, self->config != NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_asr_manager_constructed (GObject *object)
{
  PtAsrManager *self = PT_ASR_MANAGER (object);

  self->extension_set = peas_extension_set_new (self->engine,
                                                PT_TYPE_SPEECH_RECOGNIZER,
                                                NULL);
  peas_extension_set_foreach (self->extension_set,
                              (PeasExtensionSetForeachFunc) on_extension_added,
                              NULL);
  g_signal_connect (self->extension_set, "extension-added",
                    G_CALLBACK (on_extension_added), self);
  g_signal_connect (self->extension_set, "extension-removed",
                    G_CALLBACK (on_extension_removed), NULL);

  if (self->config != NULL)
    {
      pt_asr_manager_load_plugin_async (self, NULL, NULL, self);
    }

  G_OBJECT_CLASS (pt_asr_manager_parent_class)->constructed (object);
}

static void
pt_asr_manager_init (PtAsrManager *self)
{
  self->settings = g_settings_new (APP_ID);

  uint n_items;

  self->engine = peas_engine_get_default ();
  peas_engine_add_search_path (self->engine,
                               "resource:///plugins",
                               "resource:///plugins");
  peas_engine_rescan_plugins (self->engine);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->engine));
  self->have_asr = FALSE;

  for (uint i = 0; i < n_items; i++)
    {
      PeasPluginInfo *plugin_info = g_list_model_get_item (G_LIST_MODEL (self->engine), i);
      const char     *module_name = peas_plugin_info_get_module_name (plugin_info);
      const char     *category = peas_plugin_info_get_external_data (plugin_info, "Category");
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                        "MESSAGE", "Found plugin %s in category %s", module_name, category);

      if (!self->have_asr && g_strcmp0 (category, "asr") == 0)
        {
          self->have_asr = TRUE;
        }

      g_object_unref (plugin_info);
    }

  pt_asr_manager_load_config (self);
}

static void
pt_asr_manager_class_init (PtAsrManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = pt_asr_manager_set_property;
  G_OBJECT_CLASS (klass)->get_property = pt_asr_manager_get_property;
  G_OBJECT_CLASS (klass)->constructed = pt_asr_manager_constructed;
  G_OBJECT_CLASS (klass)->dispose = pt_asr_manager_dispose;

  /**
   * PtAsrManager:player:
   *
   * A reference on a PtPlayer is needed, so that ASR plugins get audio input.
   */
  obj_properties[PROP_PLAYER] =
      g_param_spec_object (
          "player", NULL, NULL,
          PT_TYPE_PLAYER,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtAsrManager:text-view:
   *
   * A reference on parent window.
   */
  obj_properties[PROP_TEXT_VIEW] =
      g_param_spec_object (
          "text-view", NULL, NULL,
          PT_TYPE_TEXT_VIEW,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtAsrManager:asr-controls:
   *
   * A reference on parent window.
   */
  obj_properties[PROP_ASR_CONTROLS] =
      g_param_spec_object (
          "asr-controls", NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtAsrManager:have-asr:
   *
   * If TRUE there are ASR plugins available, if FALSE the program has no ASR capabilities.
   * This is checked only once on startup and doesn't change anymore.
   */
  obj_properties[PROP_HAVE_ASR] =
      g_param_spec_boolean (
          "have-asr", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtAsrManager:have-config:
   *
   * If TRUE there is a valid config selected.
   */
  obj_properties[PROP_HAVE_CONFIG] =
      g_param_spec_boolean (
          "have-config", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_asr_manager_new:
 * @player: a player to get audio data from
 * @text_view: a text view to output recognized text
 * @asr_controls: box to insert widgets
 *
 * Creates a new `PtAsrManager`.
 *
 * Returns: a new #PtAsrManager
 */
PtAsrManager *
pt_asr_manager_new (PtPlayer   *player,
                    PtTextView *text_view,
                    GtkWidget  *asr_controls)
{
  g_return_val_if_fail (PT_IS_PLAYER (player), NULL);
  g_return_val_if_fail (PT_IS_TEXT_VIEW (text_view), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (asr_controls), NULL);

  return g_object_new (PT_TYPE_ASR_MANAGER,
                       "player", player,
                       "text-view", text_view,
                       "asr-controls", asr_controls,
                       NULL);
}
