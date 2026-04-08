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

#include <gio/gio.h>
#include <libpeas.h>

struct _PtAsrManager
{
  GObject parent_instance;

  PtPlayer         *player;
  PtTextView       *text_view;
  PeasEngine       *engine;
  PeasPluginInfo   *plugin;
  PeasExtensionSet *extension_set;
  PtConfig         *config;
  gboolean          have_asr;
};

enum
{
  PROP_PLAYER = 1,
  PROP_TEXT_VIEW,
  PROP_HAVE_ASR,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_TYPE (PtAsrManager, pt_asr_manager, G_TYPE_OBJECT)

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

  self->plugin = info;
  if (!pt_config_apply (self->config, G_OBJECT (plugin), &error))
    {
      g_task_return_error (task, error);
      return;
    }

  pt_speech_recognizer_load (plugin);
  g_task_return_boolean (task, TRUE);
}

static void
on_extension_removed (PeasExtensionSet   *set,
                      PeasPluginInfo     *info,
                      PtSpeechRecognizer *plugin)
{
  pt_speech_recognizer_unload (plugin);
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
pt_asr_manager_activate_finish (PtAsrManager *self,
                                GAsyncResult *result,
                                GError      **error)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
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
pt_asr_manager_activate_async (PtAsrManager       *self,
                               PtConfig           *config,
                               GCancellable       *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer            user_data)
{
  g_return_if_fail (PT_IS_ASR_MANAGER (self));
  g_return_if_fail (PT_IS_CONFIG (config));
  g_return_if_fail (self->engine != NULL);
  g_return_if_fail (self->player != NULL);
  g_return_if_fail (self->text_view != NULL);

  GTask          *task;
  char           *plugin_name;
  PeasPluginInfo *plugin_info;

  task = g_task_new (self, cancellable, callback, user_data);
  plugin_name = pt_config_get_plugin (config);
  plugin_info = peas_engine_get_plugin_info (self->engine, plugin_name);

  if (plugin_info == NULL)
    {
      g_task_return_new_error (task, PT_ERROR,
                               PT_ERROR_PLUGIN_NOT_FOUND,
                               "Plugin %s not found", plugin_name);
      g_object_unref (task);
      return;
    }

  self->config = config;

  /* Unload other plugin first */
  if (self->plugin != NULL && self->plugin != plugin_info)
    {
      peas_engine_unload_plugin (self->engine, self->plugin);
      self->plugin = NULL;
    }

  /* A plugin is already loaded */
  if (self->plugin != NULL)
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
  gboolean result;

  plugin_name = pt_config_get_plugin (config);
  if (plugin_name == NULL)
    return FALSE;

  PeasPluginInfo *info = peas_engine_get_plugin_info (self->engine, plugin_name);
  if (info == NULL)
    result = FALSE;
  else
    result = TRUE;
  // result = peas_engine_get_plugin_info (self->engine, plugin_name) != NULL;

  if (!result)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Plugin %s not available for %s", plugin_name, pt_config_get_path (config));
    }

  return result;
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

static void
pt_asr_manager_dispose (GObject *object)
{
  PtAsrManager *self = PT_ASR_MANAGER (object);

  if (self->engine != NULL)
    {
      if (self->plugin != NULL)
        {
          peas_engine_unload_plugin (self->engine, self->plugin);
        }
      peas_engine_garbage_collect (self->engine);
    }
  g_clear_object (&self->extension_set);
  g_clear_object (&self->plugin);
  g_clear_object (&self->config);
  g_clear_object (&self->player);
  g_clear_object (&self->text_view);

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
      self->player = g_value_dup_object (value);
      break;
    case PROP_TEXT_VIEW:
      self->text_view = g_value_dup_object (value);
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
    case PROP_HAVE_ASR:
      g_value_set_boolean (value, self->have_asr);
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
                                                "player", self->player,
                                                "textview", self->text_view,
                                                NULL);
  peas_extension_set_foreach (self->extension_set,
                              (PeasExtensionSetForeachFunc) on_extension_added,
                              NULL);
  g_signal_connect (self->extension_set, "extension-added",
                    G_CALLBACK (on_extension_added), self);
  g_signal_connect (self->extension_set, "extension-removed",
                    G_CALLBACK (on_extension_removed), NULL);

  G_OBJECT_CLASS (pt_asr_manager_parent_class)->constructed (object);
}

static void
pt_asr_manager_init (PtAsrManager *self)
{
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
   * PtAsrManager:textview:
   *
   * A reference on a PtTextView where ASR plugins write their text to.
   */
  obj_properties[PROP_TEXT_VIEW] =
      g_param_spec_object (
          "textview", NULL, NULL,
          PT_TYPE_TEXT_VIEW,
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

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_asr_manager_new:
 * @player: a player to get audio data from
 * @text_view: a text view to output recognized text
 *
 * Creates a new `PtAsrManager`.
 *
 * Returns: a new #PtAsrManager
 */
PtAsrManager *
pt_asr_manager_new (PtPlayer   *player,
                    PtTextView *text_view)
{
  g_return_val_if_fail (PT_IS_PLAYER (player), NULL);
  g_return_val_if_fail (PT_IS_TEXT_VIEW (text_view), NULL);

  return g_object_new (PT_TYPE_ASR_MANAGER,
                       "player", player,
                       "textview", text_view,
                       NULL);
}
