/* Copyright (C) Gabor Karsay 2023 <gabor.karsay@gmx.at>
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
#include "pt-config-list.h"

struct _PtConfigList
{
  GObject parent;

  GListStore *store;           /* actual config store              */
  GFile *config_folder;        /* user’s config folder             */
  GtkDirectoryList *file_list; /* lists config files               */
  PtPlayer *player;            /* determines if config is loadable */
  GSettings *editor;           /* gets active config               */
  gchar *active_path;          /* active config’s file path        */
  gchar *env_lang;             /* user’s locale for sorting        */
};

static void pt_config_list_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (PtConfigList, pt_config_list, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, pt_config_list_iface_init))

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
                         guint position)
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

static int
sort_configs (gconstpointer p1,
              gconstpointer p2,
              gpointer user_data)
{
  /* 1st sort order: 1) Active
   *                 2) Installed
   *                 3) Not installed */

  PtConfig *c1 = PT_CONFIG (p1);
  PtConfig *c2 = PT_CONFIG (p2);
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  GFile *file;
  gchar *path;
  int left = 0;
  int right = 0;
  int comp;
  gchar *str1, *str2;
  gboolean active, prefix1, prefix2;

  if (self->active_path[0])
    {
      file = pt_config_get_file (c1);
      path = g_file_get_path (file);
      active = g_strcmp0 (path, self->active_path) == 0;
      g_free (path);
      if (active)
        return -1;

      file = pt_config_get_file (c2);
      path = g_file_get_path (file);
      active = g_strcmp0 (path, self->active_path) == 0;
      g_free (path);
      if (active)
        return 1;
    }

  if (pt_config_is_installed (c1))
    left = 1;

  if (pt_config_is_installed (c2))
    right = 1;

  if (left > right)
    return -1;

  if (left < right)
    return 1;

  /* 2nd sort order: Own locale language before other languages */

  if (self->env_lang)
    {
      str1 = pt_config_get_lang_code (c1);
      str2 = pt_config_get_lang_code (c2);
      prefix1 = g_str_has_prefix (self->env_lang, str1);
      prefix2 = g_str_has_prefix (self->env_lang, str2);

      comp = 0;
      if (prefix1 && !prefix2)
        comp = -1;
      if (!prefix1 && prefix2)
        comp = 1;

      if (comp != 0)
          return comp;
    }

  /* 3rd sort order: Alphabetically by language name */

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
file_list_items_changed_cb (GListModel *list,
                            guint position,
                            guint removed,
                            guint added,
                            gpointer user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  GFileInfo *info;
  GFile *file;
  gchar *path;
  PtConfig *config;
  guint n_files = 0;

  n_files = g_list_model_get_n_items (list);
  for (int i = 0; i < n_files; i++)
    {
      info = g_list_model_get_item (list, i);
      file = G_FILE (g_file_info_get_attribute_object (info, "standard::file"));
      path = g_file_get_path (file);
      if (g_str_has_suffix (path, ".asr"))
        {
          config = pt_config_new (file);
          if (pt_config_is_valid (config) && pt_player_config_is_loadable (self->player, config))
            {
              g_list_store_append (self->store, config);
              g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                                "MESSAGE", "Added file %s", path);
            }
          else
            {
              g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
                                "MESSAGE", "File %s is not valid", path);
            }
          g_object_unref (config);
        }
      g_free (path);
    }

  /* Refresh active config for sorting. */
  g_free (self->active_path);
  self->active_path = g_settings_get_string (self->editor, "asr-config");
  g_list_store_sort (self->store, sort_configs, self);

  /* We are not implementing GListModel correctly, simply use hardcoded numbers. */
  g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, 1);
}

static void
file_list_error_cb (GObject *object,
                    GParamSpec *pspec,
                    gpointer user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  const GError *error;

  error = gtk_directory_list_get_error (self->file_list);
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                    "MESSAGE", "%s", error->message);
}

static void
make_config_dir_cb (GObject *source_object,
                    GAsyncResult *res,
                    gpointer user_data)
{
  PtConfigList *self = PT_CONFIG_LIST (user_data);
  GFile *config_folder = G_FILE (source_object);
  GError *error = NULL;
  gboolean success;

  success = g_file_make_directory_finish (config_folder, res, &error);

  if (success || (!success && g_error_matches (error, G_IO_ERROR,
                                               G_IO_ERROR_EXISTS)))
    {
      self->file_list = gtk_directory_list_new (G_FILE_ATTRIBUTE_STANDARD_NAME, NULL);
      gtk_directory_list_set_monitored (self->file_list, FALSE);
      g_signal_connect (self->file_list, "items_changed",
                        G_CALLBACK (file_list_items_changed_cb), self);
      g_signal_connect (self->file_list, "notify::error",
                        G_CALLBACK (file_list_error_cb), self);
      gtk_directory_list_set_file (self->file_list, config_folder);
    }
  else
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "%s", error->message);
    }

  g_clear_error (&error);
}

static void
pt_config_list_load (PtConfigList *self)
{
  /* Make sure config dir exists. */
  g_file_make_directory_async (self->config_folder,
                               G_PRIORITY_DEFAULT,
                               NULL, /* cancellable */
                               make_config_dir_cb,
                               self);
}

GFile*
pt_config_list_get_folder (PtConfigList *self)
{
  g_return_val_if_fail (PT_IS_CONFIG_LIST (self), NULL);

  return self->config_folder;
}

void
pt_config_list_refresh (PtConfigList *self)
{
  g_return_if_fail (PT_IS_CONFIG_LIST (self));

  if (!self->file_list)
    return;

  g_list_store_remove_all (G_LIST_STORE (self->store));
  gtk_directory_list_set_file (self->file_list, NULL);
  gtk_directory_list_set_file (self->file_list, self->config_folder);
}

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
pt_config_list_dispose (GObject *object)
{
  PtConfigList *self = PT_CONFIG_LIST (object);

  g_clear_object (&self->editor);
  g_clear_object (&self->config_folder);
  g_clear_object (&self->store);
  g_clear_object (&self->file_list);
  g_free (self->active_path);
  g_free (self->env_lang);

  G_OBJECT_CLASS (pt_config_list_parent_class)->dispose (object);
}

static void
pt_config_list_init (PtConfigList *self)
{
  gchar *path;
  const gchar *const *env_langs;

  path = g_build_path (G_DIR_SEPARATOR_S,
                       g_get_user_config_dir (),
                       PACKAGE_NAME, NULL);

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "config dir: %s", path);

  self->config_folder = g_file_new_for_path (path);
  g_free (path);

  self->store = g_list_store_new (PT_TYPE_CONFIG);
  self->editor = g_settings_new (APP_ID);

  env_langs = g_get_language_names ();
  if (env_langs[0])
    self->env_lang = g_strdup (env_langs[0]);
}

static void
pt_config_list_class_init (PtConfigListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = pt_config_list_dispose;
}

static void
pt_config_list_set_player (PtConfigList *self,
                           PtPlayer *player)
{
  self->player = player;
}

PtConfigList *
pt_config_list_new (PtPlayer *player)
{
  PtConfigList *self = g_object_new (PT_TYPE_CONFIG_LIST, NULL);
  pt_config_list_set_player (self, player);
  pt_config_list_load (self);
  return self;
}
