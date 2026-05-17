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

#include "pt-downloader.h"
#include "pt-speech-recognizer.h"
#include "pt-window.h"

#include <gio/gio.h>
#include <libpeas.h>

struct _PtAsrManager
{
  GObject parent_instance;

  PtWindow           *window;
  GtkWidget          *options_box;
  PtPlayer           *player;
  PtTextView         *text_view;
  PeasEngine         *engine;
  PeasPluginInfo     *plugin_info;
  PtSpeechRecognizer *plugin;
  gboolean            have_asr;
};

enum
{
  PROP_0,
  PROP_PLAYER,
  PROP_TEXT_VIEW,
  PROP_OPTIONS_BOX,
  PROP_HAVE_ASR,
  PROP_PLUGIN_LOADED,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_TYPE (PtAsrManager, pt_asr_manager, G_TYPE_OBJECT)

/**
 * pt_asr_manager_stop_transcribing:
 * @self: a `PtAsrManager`
 *
 * Stop transcribing.
 */
void
pt_asr_manager_stop_transcribing (PtAsrManager *self)
{
  g_return_if_fail (PT_IS_ASR_MANAGER (self));
  g_return_if_fail (PT_IS_SPEECH_RECOGNIZER (self->plugin));

  pt_player_pause (self->player);
  pt_player_set_mode (self->player, PT_MODE_PLAYBACK);
  pt_speech_recognizer_stop (PT_SPEECH_RECOGNIZER (self->plugin));
}

/**
 * pt_asr_manager_start_transcribing:
 * @self: a `PtAsrManager`
 *
 * Start transcribing.
 */
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
 * pt_asr_manager_unload_plugin:
 * @self: a `PtAsrManager`
 *
 * Unloads a currently loaded plugin.
 */
void
pt_asr_manager_unload_plugin (PtAsrManager *self)
{
  if (self->plugin_info == NULL)
    {
      return;
    }

  pt_speech_recognizer_unload (self->plugin);
  peas_engine_unload_plugin (self->engine, self->plugin_info);
  self->plugin_info = NULL;
}

/**
 * pt_asr_manager_load_plugin:
 * @self: a `PtAsrManager`
 * @config: a config to load
 * @error: error location
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
 *
 * Returns: true on success
 */
gboolean
pt_asr_manager_load_plugin (PtAsrManager *self,
                            PtConfig     *config,
                            GError      **error)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  g_return_val_if_fail (PT_IS_CONFIG (config), FALSE);
  g_return_val_if_fail (self->engine != NULL, FALSE);
  g_return_val_if_fail (self->player != NULL, FALSE);
  g_return_val_if_fail (self->text_view != NULL, FALSE);

  char           *plugin_name;
  PeasPluginInfo *plugin_info;
  gboolean        change;

  change = !PT_IS_SPEECH_RECOGNIZER (self->plugin);

  plugin_name = pt_config_get_plugin (config);
  plugin_info = peas_engine_get_plugin_info (self->engine, plugin_name);

  if (plugin_info == NULL)
    {
      g_set_error (error, PT_ERROR, PT_ERROR_PLUGIN_NOT_FOUND,
                   "Plugin %s not found", plugin_name);
      return FALSE;
    }

  /*if (peas_plugin_info_is_loaded (plugin_info))
    return TRUE;*/

  /* Unload previous plugin first */
  pt_asr_manager_unload_plugin (self);
  peas_engine_load_plugin (self->engine, plugin_info);
  GObject *extension = peas_engine_create_extension (self->engine, plugin_info, PT_TYPE_SPEECH_RECOGNIZER, NULL);

  if (!pt_config_configure_plugin (config, G_OBJECT (extension), error))
    return FALSE;

  self->plugin_info = plugin_info;
  self->plugin = PT_SPEECH_RECOGNIZER (extension);
  pt_speech_recognizer_load (self->plugin, self->player, self->text_view, self->options_box);

  if (change)
    g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_PLUGIN_LOADED]);
  return TRUE;
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
 * pt_asr_manager_plugin_loaded:
 * @self: a `PtAsrManager`
 *
 * See #PtAsrManager::plugin-loaded.
 * on the system.
 *
 * Returns: True if ASR is available.
 */
gboolean
pt_asr_manager_plugin_loaded (PtAsrManager *self)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (self), FALSE);
  return PT_IS_SPEECH_RECOGNIZER (self->plugin);
}

static void
pt_asr_manager_dispose (GObject *object)
{
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
    case PROP_OPTIONS_BOX:
      self->options_box = g_value_get_object (value);
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
    case PROP_OPTIONS_BOX:
      g_value_set_object (value, self->options_box);
      break;
    case PROP_HAVE_ASR:
      g_value_set_boolean (value, self->have_asr);
      break;
    case PROP_PLUGIN_LOADED:
      g_value_set_boolean (value, PT_IS_SPEECH_RECOGNIZER (self->plugin));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_asr_manager_init (PtAsrManager *self)
{
  uint n_items;

  self->engine = peas_engine_get_default ();
  peas_engine_add_search_path (self->engine,
                               "resource:///plugins",
                               "resource:///plugins");
  peas_engine_add_search_path (self->engine,
                               LIBDIR "/parlatype/plugins",
                               LIBDIR "/parlatype/plugins");
  peas_engine_enable_loader (self->engine, "python");
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
   * PtAsrManager:options-box:
   *
   * A reference on parent window.
   */
  obj_properties[PROP_OPTIONS_BOX] =
      g_param_spec_object (
          "options-box", NULL, NULL,
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
   * PtAsrManager:plugin-loaded:
   *
   * If TRUE there is a valid config selected.
   */
  obj_properties[PROP_PLUGIN_LOADED] =
      g_param_spec_boolean (
          "plugin-loaded", NULL, NULL,
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
 * @options_box: box to insert widgets
 *
 * Creates a new `PtAsrManager`.
 *
 * Returns: a new #PtAsrManager
 */
PtAsrManager *
pt_asr_manager_new (PtPlayer   *player,
                    PtTextView *text_view,
                    GtkWidget  *options_box)
{
  g_return_val_if_fail (PT_IS_PLAYER (player), NULL);
  g_return_val_if_fail (PT_IS_TEXT_VIEW (text_view), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (options_box), NULL);

  return g_object_new (PT_TYPE_ASR_MANAGER,
                       "player", player,
                       "text-view", text_view,
                       "options-box", options_box,
                       NULL);
}
