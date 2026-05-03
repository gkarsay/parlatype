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
 */

/**
 * PtDownloadManager:
 *
 * Loads the required downloader plugin and handles the download process.
 * Actual downloading is done in the plugin.
 *
 * Since: 5.0
 */

#define G_LOG_DOMAIN "pt-download-manager"

#include "config.h"

#include "pt-download-manager.h"

#include "pt-downloader.h"

#include <gio/gio.h>
#include <libpeas.h>

struct _PtDownloadManager
{
  GObject parent_instance;

  PeasEngine     *engine;
  PeasPluginInfo *plugin_info;
  PtConfig       *config;
  float           progress;
  gboolean        busy;
  PtDownloadBar  *bar;
};

enum
{
  PROP_0,
  PROP_BUSY,
  PROP_PROGRESS,
  PROP_CONFIG,
  PROP_BAR,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_TYPE (PtDownloadManager, pt_download_manager, G_TYPE_OBJECT)

static void
download_cb (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  GError            *error = NULL;
  GTask             *task = g_object_get_data (source_object, "TASK");
  PtDownloadManager *self = PT_DOWNLOAD_MANAGER (g_task_get_task_data (task));
  char              *install_folder;

  if (!pt_downloader_download_finish (PT_DOWNLOADER (source_object), res, &error))
    {
      g_task_return_error (task, error);
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  install_folder = g_build_path (G_DIR_SEPARATOR_S,
                                 g_get_user_data_dir (),
                                 "parlatype",
                                 pt_config_get_id (self->config),
                                 NULL);

  pt_config_set_base_folder (self->config, install_folder);

  self->progress = 0.0;
  self->busy = FALSE;
  peas_engine_unload_plugin (self->engine, self->plugin_info);
  g_clear_object (&self->plugin_info);
  g_clear_object (&self->config);
  g_free (install_folder);
}

/**
 * pt_download_manager_download_finish:
 * @self: a `PtDownloadManager`
 * @result: a `GAsyncResult`
 * @error:
 *
 * Finishes the [method@Pt.DownloadManager.download_async] call.
 * Returns: True on success.
 */
gboolean
pt_download_manager_download_finish (PtDownloadManager *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD_MANAGER (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
load_peas_engine (PtDownloadManager *self)
{
  self->engine = peas_engine_get_default ();
  peas_engine_add_search_path (self->engine,
                               "resource:///plugins",
                               "resource:///plugins");
  peas_engine_add_search_path (self->engine,
                               LIBDIR "/parlatype/plugins",
                               LIBDIR "/parlatype/plugins");
  peas_engine_enable_loader (self->engine, "python");
  peas_engine_rescan_plugins (self->engine);
}

/**
 * pt_download_manager_download_async:
 * @self: a `PtDownloadManager`
 * @config: a config with download data
 * @cancellable: A `GCancellable` to cancel the operation.
 * @callback: A callback to call when the operation is complete.
 * @user_data: Data to pass to `callback`.
 *
 * Loads the plugin referenced in PtConfig, applies all properties of PtConfig
 * to the plugin and makes it ready for action.
 *
 */
void
pt_download_manager_download_async (PtDownloadManager  *self,
                                    PtConfig           *config,
                                    GCancellable       *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer            user_data)
{
  g_return_if_fail (PT_IS_DOWNLOAD_MANAGER (self));
  g_return_if_fail (PT_IS_CONFIG (config));
  g_return_if_fail (!self->busy);

  GTask          *task;
  char           *plugin_name;
  PeasPluginInfo *plugin_info;

  if (self->engine == NULL)
    load_peas_engine (self);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, self, NULL);
  plugin_name = pt_config_get_downloader (config);
  plugin_info = peas_engine_get_plugin_info (self->engine, plugin_name);

  if (plugin_info == NULL)
    {
      g_task_return_new_error (task, PT_ERROR,
                               PT_ERROR_PLUGIN_NOT_FOUND,
                               "Plugin %s not found", plugin_name);
      g_object_unref (task);
      return;
    }

  if (peas_plugin_info_is_loaded (plugin_info))
    {
      g_task_return_new_error (task, PT_ERROR,
                               PT_ERROR_PLUGIN_NOT_FOUND,
                               "Plugin %s in use", plugin_name);
      g_object_unref (task);
      return;
    }

  peas_engine_load_plugin (self->engine, plugin_info);
  GObject *extension = peas_engine_create_extension (self->engine, plugin_info, PT_TYPE_DOWNLOADER, NULL);
  pt_config_configure_downloader (config, extension, NULL);

  self->plugin_info = plugin_info;
  self->config = g_object_ref (config);
  self->busy = TRUE;
  self->progress = 0.0;

  pt_download_bar_set_name (self->bar, pt_config_get_name (self->config));
  pt_download_bar_set_total (self->bar, pt_config_get_download_size (self->config));
  pt_download_bar_set_cancellable (self->bar, cancellable);
  pt_download_bar_set_revealed (self->bar, TRUE);

  g_object_bind_property (extension, "current",
                          self->bar, "current",
                          G_BINDING_SYNC_CREATE);

  g_object_set_data_full (extension,
                          "TASK",
                          g_steal_pointer (&task),
                          g_object_unref);

  pt_downloader_download_async (PT_DOWNLOADER (extension), cancellable, download_cb, self);
}

/**
 * pt_download_manager_has_plugin:
 * @self: a `PtDownloadManager`
 * @config: a PtConfig
 *
 * Reads the plugin name of the config and checks whether this plugin is available
 * on the system.
 *
 * Returns: TRUE for an available plugin.
 */
gboolean
pt_download_manager_has_plugin (PtDownloadManager *self,
                                PtConfig          *config)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD_MANAGER (self), FALSE);
  g_return_val_if_fail (PT_IS_CONFIG (config), FALSE);

  char    *plugin_name;
  gboolean success;

  if (self->engine == NULL)
    load_peas_engine (self);

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

static void
pt_download_manager_dispose (GObject *object)
{
  PtDownloadManager *self = PT_DOWNLOAD_MANAGER (object);

  if (self->engine != NULL)
    {
      if (self->plugin_info != NULL)
        {
          peas_engine_unload_plugin (self->engine, self->plugin_info);
        }
      peas_engine_garbage_collect (self->engine);
    }
  g_clear_object (&self->plugin_info);
  g_clear_object (&self->config);

  G_OBJECT_CLASS (pt_download_manager_parent_class)->dispose (object);
}

static void
pt_download_manager_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  PtDownloadManager *self = PT_DOWNLOAD_MANAGER (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      self->config = g_value_get_object (value);
      break;
    case PROP_BAR:
      self->bar = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_manager_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  PtDownloadManager *self = PT_DOWNLOAD_MANAGER (object);

  switch (property_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, self->busy);
      break;
    case PROP_PROGRESS:
      g_value_set_float (value, self->progress);
      break;
    case PROP_CONFIG:
      g_value_set_object (value, self->config);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_manager_init (PtDownloadManager *self)
{
}

static void
pt_download_manager_class_init (PtDownloadManagerClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = pt_download_manager_set_property;
  G_OBJECT_CLASS (klass)->get_property = pt_download_manager_get_property;
  G_OBJECT_CLASS (klass)->dispose = pt_download_manager_dispose;

  /**
   * PtDownloadManager:busy:
   *
   * Whether there is an ongoing download. We dont mess with multiple downloads.
   */
  obj_properties[PROP_BUSY] =
      g_param_spec_boolean (
          "busy", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtDownloadManager:progress:
   *
   * Progress of download between 0.0 and 1.0.
   */
  obj_properties[PROP_PROGRESS] =
      g_param_spec_float (
          "progress", NULL, NULL, 0.0, 1.0,
          0.0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtDownloadManager:config:
   *
   * The current config that is downloading.
   */
  obj_properties[PROP_CONFIG] =
      g_param_spec_object (
          "config", NULL, NULL,
          PT_TYPE_CONFIG,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtDownloadManager:bar:
   *
   * The current config that is downloading.
   */
  obj_properties[PROP_BAR] =
      g_param_spec_object (
          "bar", NULL, NULL,
          PT_TYPE_DOWNLOAD_BAR,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_download_manager_new:
 * @bar: a widget to show progress
 *
 * Creates a new `PtDownloadManager`.
 *
 * Returns: a new #PtDownloadManager
 */
PtDownloadManager *
pt_download_manager_new (PtDownloadBar *bar)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD_BAR (bar), NULL);

  return g_object_new (PT_TYPE_DOWNLOAD_MANAGER, "bar", bar, NULL);
}
