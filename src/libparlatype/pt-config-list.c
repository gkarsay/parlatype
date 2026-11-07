/* Copyright 2023 Gabor Karsay <gabor.karsay@gmx.at>
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
 * PtConfigList:
 *
 * List of configurations.
 *
 * Configurations are saved as *.asr files, usually in ~/.configs/parlatype.
 * PtConfigList reads all those files, converts them to `PtConfig`s and sorts
 * them. It does not monitor the config folder nor the individual files.
 *
 * It doesn't implement GListModel correctly: the "changed-items" signal doesn't
 * report the true number of changed elements.
 */

#define G_LOG_DOMAIN "pt-config-list"

#include "config.h"

#include "pt-config-list.h"

#include "pt-config-private.h"
#include "pt-marshalers.h"
#include "pt-utils.h"

#include <libpeas.h>

struct _PtConfigList
{
  GObject parent_instance;

  GListStore       *store;         /* actual config store              */
  char             *config_path;   /* path to user’s config folder     */
  GFile            *config_folder; /* user’s config folder             */
  GtkDirectoryList *file_list;     /* lists config files               */
  GSettings        *settings;      /* gets active config               */
  gchar            *env_lang;      /* user’s locale for sorting        */
  PeasEngine       *engine;
  PtConfig         *active;
  PtAsrManager     *asr_manager;
};

enum
{
  PROP_0,
  PROP_ASR_MANAGER,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

enum
{
  SIGNAL_ERROR,
  SIGNAL_DOWNLOAD_FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void pt_config_list_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (PtConfigList, pt_config_list, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, pt_config_list_iface_init))

static GType
pt_config_list_get_item_type (GListModel *list)
{
  return PT_TYPE_CONFIG;
}

static guint
pt_config_list_get_n_items (GListModel *list)
{
  PtConfigList *self = PT_CONFIG_LIST (list);
  return g_list_model_get_n_items (G_LIST_MODEL (self->store));
}

static gpointer
pt_config_list_get_item (GListModel *list,
                         guint       position)
{
  PtConfigList *self = PT_CONFIG_LIST (list);
  return g_list_model_get_item (G_LIST_MODEL (self->store), position);
}

static void
pt_config_list_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = pt_config_list_get_item_type;
  iface->get_n_items = pt_config_list_get_n_items;
  iface->get_item = pt_config_list_get_item;
}

static gboolean
plugin_is_available (PtConfigList *self,
                     char         *plugin_name)
{
  if (plugin_name == NULL)
    return FALSE;

  PeasPluginInfo *info = peas_engine_get_plugin_info (self->engine, plugin_name);
  return info != NULL && peas_plugin_info_is_available (info, NULL);
}

static gboolean
speech_recognizer_is_available (PtConfigList *self,
                                PtConfig     *config)
{
  char *speech_recognizer = pt_config_get_plugin (config);
  return plugin_is_available (self, speech_recognizer);
}

static gboolean
downloader_is_available (PtConfigList *self,
                         PtConfig     *config)
{
  char *downloader = pt_config_get_downloader (config);
  return plugin_is_available (self, downloader);
}

static gboolean
set_active (PtConfigList *self,
            PtConfig     *config)
{
  if (pt_config_is_valid (config) &&
      pt_config_is_installed (config) &&
      speech_recognizer_is_available (self, config))
    {
      g_clear_object (&self->active);
      self->active = g_object_ref (config);
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "config found: »%s«", pt_config_get_path (config));
      return TRUE;
    }
  else
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "config not usable: »%s«", pt_config_get_path (config));
      return FALSE;
    }
}

static void
pt_config_list_load_active_from_settings (PtConfigList *self)
{
  GFile    *asr_file;
  gchar    *asr_path;
  PtConfig *config;

  asr_path = g_settings_get_string (self->settings, "asr-config");
  asr_file = g_file_new_for_path (asr_path);
  config = pt_config_new (asr_file);
  pt_config_list_set_active (self, config);

  g_object_unref (config);
  g_object_unref (asr_file);
  g_free (asr_path);
}

/*static int
find_config_position (PtConfigList *self,
                      PtConfig     *config)
{
  if (config == NULL)
    return -1;

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self)); i++)
    {
      PtConfig *compare = g_list_model_get_item (G_LIST_MODEL (self), i);
      if (pt_config_equal (config, compare))
        return i;
    }

  return -1;
}*/

static int
sort_configs (gconstpointer p1,
              gconstpointer p2,
              gpointer      user_data)
{
  /* 1st sort order: 1) Active
   *                 2) Installed
   *                 3) Not installed */

  PtConfig     *c1 = PT_CONFIG ((void *) p1);
  PtConfig     *c2 = PT_CONFIG ((void *) p2);
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  int           left = 0;
  int           right = 0;
  int           comp;
  gchar        *str1, *str2;
  gboolean      prefix1, prefix2;

  if (pt_config_is_active (c1))
    return -1;

  if (pt_config_is_active (c2))
    return 1;

  if (pt_config_is_installed (c1))
    left = 1;

  if (pt_config_is_installed (c2))
    right = 1;

  if (left > right)
    return -1;

  if (left < right)
    return 1;

  /* 2nd sort order: Own locale language before other languages */

  str1 = pt_config_get_lang_code (c1);
  str2 = pt_config_get_lang_code (c2);

  if (self->env_lang)
    {
      prefix1 = g_str_has_prefix (self->env_lang, str1);
      prefix2 = g_str_has_prefix (self->env_lang, str2);

      if (prefix1 && !prefix2)
        return -1;
      if (!prefix1 && prefix2)
        return 1;
    }

  /* 3rd sort order: Multilingual models before others */

  prefix1 = g_strcmp0 (str1, "*") == 0;
  prefix2 = g_strcmp0 (str2, "*") == 0;

  if (prefix1 && !prefix2)
    return -1;
  if (!prefix1 && prefix2)
    return 1;

  /* 4th sort order: Alphabetically by language name */

  str1 = pt_config_get_lang_name (c1);
  str2 = pt_config_get_lang_name (c2);

  comp = g_strcmp0 (str1, str2);
  if (comp != 0)
    return comp;

  /* 4th sort order: Alphabetically by name */

  str1 = pt_config_get_name (c1);
  str2 = pt_config_get_name (c2);

  return g_strcmp0 (str1, str2);
}

static void
pt_config_list_set_downloadable (PtConfigList *self,
                                 PtConfig     *config)
{
  gboolean downloadable = FALSE;

  if (!pt_config_is_installed (config))
    downloadable = downloader_is_available (self, config);

  _pt_config_set_downloadable (config, downloadable);
}

static void
download_finished_cb (PtConfig *config,
                      GError   *error,
                      gpointer  user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);

  /* Propagate signal to PtWindow. */
  g_signal_emit (self, signals[SIGNAL_DOWNLOAD_FINISHED], 0, error, pt_config_get_name (config));
}

static void
file_list_items_changed_cb (GListModel *list,
                            guint       position,
                            guint       removed,
                            guint       added,
                            gpointer    user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  GFileInfo    *info;
  GFile        *file;
  gchar        *path;
  PtConfig     *config;
  guint         n_files;
  guint         n_removed;
  guint         n_added = 0;

  n_removed = pt_config_list_get_n_items (G_LIST_MODEL (self));
  g_list_store_remove_all (G_LIST_STORE (self->store));

  n_files = g_list_model_get_n_items (list);
  for (uint i = 0; i < n_files; i++)
    {
      info = g_list_model_get_item (list, i);
      file = G_FILE (g_file_info_get_attribute_object (info, "standard::file"));
      path = g_file_get_path (file);
      if (g_str_has_suffix (path, ".asr"))
        {
          config = pt_config_new (file);
          if (pt_config_is_valid (config) && speech_recognizer_is_available (self, config))
            {
              pt_config_list_set_downloadable (self, config);
              g_signal_connect (config, "download-finished", G_CALLBACK (download_finished_cb), self);
              if (pt_config_equal (config, self->active))
                {
                  _pt_config_set_active (config, TRUE);
                }
              g_list_store_append (self->store, config);
              n_added++;
              g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                                "MESSAGE", "PtConfigList new file: %s", path);
            }
          else
            {
              g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                                "MESSAGE", "PtConfigList file invalid: %s", path);
            }
          g_object_unref (config);
        }
      g_free (path);
    }

  g_list_store_sort (self->store, sort_configs, self);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_removed, n_added);
}

static void
file_list_loading_cb (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  gboolean      loading = gtk_directory_list_is_loading (self->file_list);

  /* If finished loading and there are no items, emit changed signal.
   * Needed to notify that all items have been removed. */
  if (!loading && g_list_model_get_n_items (G_LIST_MODEL (self->file_list)) == 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, 1, 0);
}

static void
file_list_error_cb (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  const GError *error;

  error = gtk_directory_list_get_error (self->file_list);
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                    "MESSAGE", "%s", error->message);
}

static void
create_file_list (PtConfigList *self)
{
  self->file_list = gtk_directory_list_new (G_FILE_ATTRIBUTE_STANDARD_NAME, NULL);
  gtk_directory_list_set_monitored (self->file_list, FALSE);
  g_signal_connect (self->file_list, "items_changed",
                    G_CALLBACK (file_list_items_changed_cb), self);
  g_signal_connect (self->file_list, "notify::loading",
                    G_CALLBACK (file_list_loading_cb), self);
  g_signal_connect (self->file_list, "notify::error",
                    G_CALLBACK (file_list_error_cb), self);
  gtk_directory_list_set_file (self->file_list, self->config_folder);
}

static void
copy_configs_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  GError       *error = NULL;

  if (pt_utils_copy_configs_finish (result, &error))
    {
      create_file_list (self);
    }
  else
    {
      g_signal_emit (self, signals[SIGNAL_ERROR], 0, error);
      g_error_free (error);
    }
}

static void
make_config_dir_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  GFile        *config_folder = G_FILE (source_object);
  GError       *error = NULL;
  gboolean      success;

  success = g_file_make_directory_finish (config_folder, res, &error);

  if (success || (!success && g_error_matches (error, G_IO_ERROR,
                                               G_IO_ERROR_EXISTS)))
    {
      pt_utils_copy_configs_async (ASR_DIR,
                                   self->config_path,
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback) copy_configs_cb,
                                   self);
    }
  else
    {
      g_signal_emit (self, signals[SIGNAL_ERROR], 0, error);
      g_clear_error (&error);
    }
}

/**
 * pt_config_list_get_active:
 * @self: a `PtConfigList`
 *
 * Returns the currently selected config.
 *
 * Returns: (transfer none): the currently selected config.
 */
PtConfig *
pt_config_list_get_active (PtConfigList *self)
{
  g_return_val_if_fail (PT_IS_CONFIG_LIST (self), NULL);

  return self->active;
}

/**
 * pt_config_list_set_active:
 * @self: a `PtConfigList`
 * @config: (nullable): a PtConfig
 *
 * - Sets the current config or unsets it with NULL.
 * - Loads or unload the corresponding plugin.
 * - Persists choice in settings.
 *
 * Returns: TRUE for success.
 */
gboolean
pt_config_list_set_active (PtConfigList *self,
                           PtConfig     *config)
{
  gboolean success;

  g_return_val_if_fail (PT_IS_CONFIG_LIST (self), FALSE);
  g_return_val_if_fail (PT_IS_CONFIG (config) || config == NULL, FALSE);

  if (self->active == NULL && config == NULL)
    return TRUE;

  if (config == NULL)
    {
      success = g_settings_set_string (self->settings, "asr-config", "");
      if (success)
        {
          g_clear_object (&self->active);
          pt_asr_manager_unload_plugin (self->asr_manager);
          return TRUE;
        }
    }

  if (pt_config_equal (self->active, config))
    return TRUE;

  if (set_active (self, config))
    {
      char *path = pt_config_get_path (config);
      g_settings_set_string (self->settings, "asr-config", path);
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "asr-config set to %s", path);
      pt_asr_manager_load_plugin (self->asr_manager, config, NULL);
      return TRUE;
    }

  return FALSE;
}

/**
 * pt_config_list_get_folder:
 * @self: a `PtConfigList`
 *
 * Returns the folder where the user's configurations are saved.
 * Usually that is ~/.configs/parlatype
 *
 * Returns: (transfer none): a folder as `GFile`
 */
GFile *
pt_config_list_get_folder (PtConfigList *self)
{
  g_return_val_if_fail (PT_IS_CONFIG_LIST (self), NULL);

  return self->config_folder;
}

/**
 * pt_config_list_refresh:
 * @self: a `PtConfigList`
 *
 * Rebuilds the list and emits an "items-changed" signal.
 */
void
pt_config_list_refresh (PtConfigList *self)
{
  g_return_if_fail (PT_IS_CONFIG_LIST (self));

  if (!self->file_list)
    return;

  create_file_list (self);
}

/**
 * pt_config_list_sort:
 * @self: a PtConfigList
 *
 * Sorts the list and emits an "items-changed" signal.
 */
void
pt_config_list_sort (PtConfigList *self)
{
  g_return_if_fail (PT_IS_CONFIG_LIST (self));

  if (!self->file_list)
    return;

  g_list_store_sort (self->store, sort_configs, self);

  /* We are not implementing GListModel correctly, simply use hardcoded numbers. */
  g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, 1);
}

static void
pt_config_list_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PtConfigList *self = PT_CONFIG_LIST (object);

  switch (property_id)
    {
    case PROP_ASR_MANAGER:
      self->asr_manager = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_config_list_dispose (GObject *object)
{
  PtConfigList *self = PT_CONFIG_LIST (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->config_folder);
  g_clear_object (&self->store);
  g_clear_object (&self->file_list);
  g_clear_object (&self->active);
  g_free (self->env_lang);
  g_free (self->config_path);

  G_OBJECT_CLASS (pt_config_list_parent_class)->dispose (object);
}

static void
pt_config_list_constructed (GObject *object)
{
  PtConfigList *self = PT_CONFIG_LIST (object);

  pt_config_list_load_active_from_settings (self);

  if (pt_utils_upgraded ())
    {
      /* On first run or upgrade:
       * 1. Make sure config folder exists.
       * 2. Copy configs from installation folder to user's config folder.
       * 3. Load list. */

      g_file_make_directory_async (self->config_folder,
                                   G_PRIORITY_DEFAULT,
                                   NULL, /* cancellable */
                                   make_config_dir_cb,
                                   self);
    }
  else
    {
      create_file_list (self);
    }

  G_OBJECT_CLASS (pt_config_list_parent_class)->constructed (object);
}

static void
pt_config_list_init (PtConfigList *self)
{
  const gchar *const *env_langs;

  self->config_path = g_build_path (G_DIR_SEPARATOR_S,
                                    g_get_user_config_dir (),
                                    PACKAGE_NAME, NULL);
  self->config_folder = g_file_new_for_path (self->config_path);

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "config dir: %s", self->config_path);

  self->store = g_list_store_new (PT_TYPE_CONFIG);
  self->settings = g_settings_new (APP_ID);

  env_langs = g_get_language_names ();
  if (env_langs[0])
    self->env_lang = g_strdup (env_langs[0]);

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

static void
pt_config_list_class_init (PtConfigListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = pt_config_list_set_property;
  object_class->constructed = pt_config_list_constructed;
  object_class->dispose = pt_config_list_dispose;

  /**
   * PtConfigList::error:
   * @self: the list emitting the signal
   * @error: a GError
   *
   * The #PtConfigList::error signal is emitted on file operations.
   */
  signals[SIGNAL_ERROR] = g_signal_new ("error",
                                        PT_TYPE_CONFIG_LIST,
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL,
                                        NULL,
                                        g_cclosure_marshal_VOID__BOXED,
                                        G_TYPE_NONE,
                                        1, G_TYPE_ERROR);

  /**
   * PtConfigList::download-finished:
   * @self: the instance emitting the signal
   * @error: a GError or NULL
   * @name: config display name
   *
   * The #PtDownload::finished signal is emitted after download success or failure.
   * In case of failure, there is en error set; in case of success the error is NULL.
   */
  signals[SIGNAL_DOWNLOAD_FINISHED] = g_signal_new ("download-finished",
                                                    PT_TYPE_CONFIG_LIST,
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    _pt_cclosure_marshal_VOID__BOXED_STRING,
                                                    G_TYPE_NONE,
                                                    2, G_TYPE_ERROR, G_TYPE_STRING);

  /**
   * PtConfigList:asr-manager:
   *
   * A reference on a PtPlayer is needed, so that ASR plugins get audio input.
   */
  obj_properties[PROP_ASR_MANAGER] =
      g_param_spec_object (
          "asr-manager", NULL, NULL,
          PT_TYPE_ASR_MANAGER,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_config_list_new:
 * @asr_manager: a PtAsrManager
 *
 * A list of configurations.
 *
 * Returns: a newly created PtConfigList, already populated and sorted
 */
PtConfigList *
pt_config_list_new (PtAsrManager *asr_manager)
{
  g_return_val_if_fail (PT_IS_ASR_MANAGER (asr_manager), NULL);

  return g_object_new (PT_TYPE_CONFIG_LIST,
                       "asr-manager", asr_manager,
                       NULL);
}
