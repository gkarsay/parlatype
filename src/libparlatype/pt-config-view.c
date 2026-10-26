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
 * PtConfigView:
 *
 * A dialog for a single ASR configuration.
 *
 * It presents most fields of a `PtConfig`, some of them can be changed, and
 * allows for activating, deactivating or deleting this config.
 */

#include "config.h"

#include "pt-config-view.h"

#include "pt-config-list.h"
#include "pt-download-row.h"
#include "pt-downloader.h"
#include "pt-preferences.h"
#include "pt-prefs-install-row.h"
#include "pt-utils.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libpeas.h>
#include <parlatype.h>

struct _PtConfigView
{
  AdwBin parent_instance;

  PtConfig     *config;
  PtConfigList *config_list;
  PtDownload   *download;
  PtDownloader *downloader;

  GtkBox              *page;
  AdwStatusPage       *status_page;
  GtkWidget           *name_row;
  AdwActionRow        *language_row;
  AdwActionRow        *plugin_row;
  AdwPreferencesGroup *info_group;
  AdwPreferencesGroup *install_group;
  GtkWidget           *download_group;
  PtDownloadRow       *download_row;
  GtkRevealer         *download_revealer;

  int download_total;

  GBinding *binding1;
  GBinding *binding2;
  gulong    conn_id_downloader;
  gulong    conn_id_download;
  gulong    conn_id_config;
};

enum
{
  PROP_0,
  PROP_CONFIG,
  PROP_CONFIG_LIST,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtConfigView, pt_config_view, ADW_TYPE_BIN)

static void
name_row_apply_cb (GtkEditable *editable,
                   gpointer     user_data)
{
  PtConfigView *self = PT_CONFIG_VIEW (user_data);
  gchar        *old_name, *new_name;

  old_name = pt_config_get_name (self->config);
  new_name = g_strdup (gtk_editable_get_text (editable));
  new_name = g_strchug (new_name);
  new_name = g_strchomp (new_name);

  if (g_strcmp0 (new_name, "") == 0)
    {
      gtk_editable_set_text (editable, old_name);
      g_free (new_name);
      return;
    }

  if (g_strcmp0 (new_name, old_name) != 0)
    {
      pt_config_set_name (self->config, new_name);
    }

  g_free (new_name);
}

static gboolean
have_string (gchar *str)
{
  return (str && g_strcmp0 (str, "") != 0);
}

/* GtkLinkButton with label set to host */
static GtkWidget *
pt_download_button_new (gchar *link)
{
  GtkWidget   *link_button;
  GUri        *uri;
  const gchar *host = NULL;

  uri = g_uri_parse (link, G_URI_FLAGS_NONE, NULL);
  if (uri)
    {
      host = g_uri_get_host (uri);
    }

  /* Translators: Fallback label for a download location */
  link_button = gtk_link_button_new_with_label (link, host ? host : _ ("Link"));
  g_uri_unref (uri);
  return link_button;
}

static void
installed_cb (PtConfig   *config,
              GParamSpec *pspec,
              gpointer    user_data)
{
  PtConfigView *self = PT_CONFIG_VIEW (user_data);

  /* Download row */

  if (pt_config_is_installed (self->config))
    {
      gtk_revealer_set_reveal_child (self->download_revealer, FALSE);
    }
  else
    {
      if (pt_config_get_downloader (self->config) != NULL)
        {
          // gtk_widget_set_sensitive (self->download_button, TRUE);
        }
    }
}

static void
add_info_row (PtConfigView *self,
              gchar        *title,
              gchar        *subtitle)
{
  GtkWidget *row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);
  gtk_widget_add_css_class (row, "property");
  adw_preferences_group_add (self->info_group, row);
}

/**
 * pt_config_view_set_config_list:
 * @self: a `PtConfigView`
 * @config_list: a `PtConfigList`
 *
 * Needs to be set before operating on configs.
 */
void
pt_config_view_set_config_list (PtConfigView *self,
                                PtConfigList *config_list)
{
  g_return_if_fail (PT_IS_CONFIG_VIEW (self));
  g_return_if_fail (PT_IS_CONFIG_LIST (config_list));

  self->config_list = config_list;
}

/* from ephy-download-widget.c */
static char *
get_remaining_time (guint64 content_length,
                    guint64 received_length,
                    gdouble elapsed_time)
{
  gdouble remaining_time;
  gdouble per_byte_time;

  if (elapsed_time == 0)
    return NULL;

  per_byte_time = elapsed_time / received_length;
  remaining_time = per_byte_time * (content_length - received_length);

  guint seconds = remaining_time;
  if (seconds < 60)
    {
      /* Remaining time to download an item in seconds */
      return g_strdup_printf (ngettext ("%d second left",
                                        "%d seconds left", seconds),
                              seconds);
    }
  else if (seconds < (60 * 60))
    {
      seconds /= 60;
      /* Remaining time to download an item in minutes */
      return g_strdup_printf (ngettext ("%d minute left",
                                        "%d minutes left", seconds),
                              seconds);
    }
  else
    {
      seconds /= 60 * 60;
      /* Remaining time to download an item in hours */
      return g_strdup_printf (ngettext ("%d hour left",
                                        "%d hours left", seconds),
                              seconds);
    }
}

static void
downloader_progress_cb (PtDownloader *downloader,
                        GParamSpec   *spec,
                        gpointer      user_data)
{
  PtConfigView *self = PT_CONFIG_VIEW (user_data);

  PtDownloadRow *row = PT_DOWNLOAD_ROW (self->download_row);
  int            received_bytes;
  float          progress;
  char          *received;
  char          *total;
  char          *progress_label;
  char          *remaining_label;

  g_object_get (downloader, "current", &received_bytes, NULL);
  progress = received_bytes / (float) self->download_total;
  pt_download_row_set_progress_bar (row, progress);

  received = g_format_size (received_bytes);
  total = g_format_size (self->download_total);
  progress_label = g_strdup_printf ("%s / %s", received, total);
  pt_download_row_set_progress_label (row, progress_label);

  remaining_label = get_remaining_time (self->download_total,
                                        received_bytes,
                                        pt_download_get_elapsed_time (self->download));
  pt_download_row_set_remaining_label (row, remaining_label);

  g_free (progress_label);
  g_free (remaining_label);
  g_free (total);
  g_free (received);
}

static void
download_finished_cb (PtDownload *download,
                      GError     *error,
                      gpointer    user_data)
{
  PtConfigView *self = PT_CONFIG_VIEW (user_data);

  g_clear_signal_handler (&self->conn_id_download, self->download);

  if (error != NULL)
    {
      pt_download_row_set_revealed (self->download_row, FALSE);
    }
}

static void
connect_download (PtConfigView *self)
{
  self->downloader = pt_download_get_downloader (self->download);
  self->download_total = pt_config_get_download_size (self->config);
  self->conn_id_downloader = g_signal_connect (self->downloader, "notify::current",
                                               G_CALLBACK (downloader_progress_cb), self);
  self->conn_id_download = g_signal_connect (self->download, "finished",
                                             G_CALLBACK (download_finished_cb), self);
}

static void
start_download_cb (PtConfigView *self)
{
  self->download = pt_config_create_download (self->config, NULL);
  connect_download (self);
  pt_download_start (self->download);
  pt_download_row_set_revealed (PT_DOWNLOAD_ROW (self->download_row), TRUE);
}

static void
progress_row_cancelled_cb (PtConfigView *self,
                           GtkWidget    *download_row)
{
  pt_download_cancel (self->download);
  g_clear_signal_handler (&self->conn_id_downloader, self->downloader);
}

static void
pt_config_view_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PtConfigView *self = PT_CONFIG_VIEW (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      g_clear_object (&self->config);
      self->config = g_value_dup_object (value);
      break;
    case PROP_CONFIG_LIST:
      self->config_list = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_config_view_dispose (GObject *object)
{
  PtConfigView *self = PT_CONFIG_VIEW (object);

  g_clear_signal_handler (&self->conn_id_config, self->config);
  g_clear_signal_handler (&self->conn_id_downloader, self->downloader);
  g_clear_signal_handler (&self->conn_id_download, self->download);
  g_binding_unbind (self->binding1);
  g_binding_unbind (self->binding2);
  g_clear_object (&self->config);
  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_CONFIG_VIEW);

  G_OBJECT_CLASS (pt_config_view_parent_class)->dispose (object);
}

static void
pt_config_view_constructed (GObject *object)
{
  PtConfigView *self = PT_CONFIG_VIEW (object);

  gchar               *name;
  gchar               *str;
  AdwPreferencesGroup *group;

  self->binding1 = g_object_bind_property (self->config, "name",
                                           self->status_page, "title",
                                           G_BINDING_SYNC_CREATE);

  self->binding2 = g_object_bind_property (self->config, "downloadable",
                                           self->download_group, "visible",
                                           G_BINDING_SYNC_CREATE);

  if (pt_config_is_downloadable (self->config))
    {
      int   size = pt_config_get_download_size (self->config);
      char *size_string;
      size_string = pt_utils_pretty_download_size (size);
      pt_download_row_set_size_label (self->download_row, size_string);
      g_free (size_string);
    }

  self->download = pt_config_get_download (self->config);
  if (self->download != NULL)
    connect_download (self);

  name = pt_config_get_name (self->config);
  gtk_editable_set_text (GTK_EDITABLE (self->name_row), name);
  gtk_widget_set_focus_child (GTK_WIDGET (self), NULL);

  adw_action_row_set_subtitle (self->language_row,
                               pt_config_get_lang_name (self->config));

  adw_action_row_set_subtitle (self->plugin_row,
                               pt_config_get_plugin (self->config));

  str = pt_config_get_key (self->config, "Publisher");
  if (have_string (str))
    add_info_row (self, _ ("Publisher"), str);
  g_free (str);

  str = pt_config_get_key (self->config, "License");
  if (have_string (str))
    add_info_row (self, _ ("License"), str);
  g_free (str);

  /* Installation */
  group = self->install_group;
  str = pt_config_get_key (self->config, "URL1");
  if (have_string (str))
    {
      GtkWidget *url1_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (url1_row), _ ("Download"));
      GtkWidget *link = pt_download_button_new (str);
      adw_action_row_add_suffix (ADW_ACTION_ROW (url1_row), link);
      adw_preferences_group_add (group, GTK_WIDGET (url1_row));
    }
  g_free (str);

  str = pt_config_get_key (self->config, "URL2");
  if (have_string (str))
    {
      GtkWidget *url2_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (url2_row), _ ("Download"));
      GtkWidget *link = pt_download_button_new (str);
      adw_action_row_add_suffix (ADW_ACTION_ROW (url2_row), link);
      adw_preferences_group_add (group, GTK_WIDGET (url2_row));
    }
  g_free (str);

  GtkWidget *install_row = pt_prefs_install_row_new (self->config);
  adw_preferences_group_add (group, install_row);
  self->conn_id_config = g_signal_connect (self->config, "notify::is-installed",
                                           G_CALLBACK (installed_cb), self);
  installed_cb (self->config, NULL, self);

  G_OBJECT_CLASS (pt_config_view_parent_class)->constructed (object);
}

static void
pt_config_view_init (PtConfigView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_config_view_class_init (PtConfigViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = pt_config_view_set_property;
  object_class->constructed = pt_config_view_constructed;
  object_class->dispose = pt_config_view_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/config-view.ui");
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, page);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, status_page);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, name_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, language_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, plugin_row);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, info_group);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, install_group);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, download_revealer);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, download_group);
  gtk_widget_class_bind_template_child (widget_class, PtConfigView, download_row);
  gtk_widget_class_bind_template_callback (widget_class, name_row_apply_cb);
  gtk_widget_class_bind_template_callback (widget_class, start_download_cb);
  gtk_widget_class_bind_template_callback (widget_class, progress_row_cancelled_cb);

  /**
   * PtConfigView:config:
   *
   * The configuration object.
   */
  obj_properties[PROP_CONFIG] =
      g_param_spec_object (
          "config", NULL, NULL,
          PT_TYPE_CONFIG,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfigView:config-list:
   *
   * A reference on a PtPlayer is needed, so that ASR plugins get audio input.
   */
  obj_properties[PROP_CONFIG_LIST] =
      g_param_spec_object (
          "config-list", NULL, NULL,
          PT_TYPE_CONFIG_LIST,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_config_view_new:
 * @config: a `PtConfig`
 * @config_list: xxx
 *
 * Creates a `PtConfigView` for a particular config to be shown in `PtModelDialog`.
 *
 * Returns: a newly created `PtConfigView`.
 */
PtConfigView *
pt_config_view_new (PtConfig     *config,
                    PtConfigList *config_list)
{
  g_return_val_if_fail (PT_IS_CONFIG (config), NULL);

  return g_object_new (PT_TYPE_CONFIG_VIEW,
                       "config", config,
                       "config-list", config_list,
                       NULL);
}
