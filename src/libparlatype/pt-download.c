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
 * PtDownload:
 *
 * Loads the required downloader plugin and handles the download process.
 * Actual downloading is done in the plugin.
 *
 * Since: 5.0
 */

#define G_LOG_DOMAIN "pt-download"

#include "config.h"

#include "pt-download.h"

#include "pt-downloader.h"

#include <gio/gio.h>
#include <libpeas.h>

struct _PtDownload
{
  GObject parent_instance;

  PeasEngine     *engine;
  PeasPluginInfo *plugin_info;
  float           progress;
  GCancellable   *cancellable;
  char           *display_name;
  char           *plugin_name;
  PtDownloader   *downloader;
  GTimer         *timer;
};

enum
{
  PROP_0,
  PROP_PROGRESS,
  PROP_DISPLAY_NAME,
  PROP_PLUGIN_NAME,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

enum
{
  SIGNAL_FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (PtDownload, pt_download, G_TYPE_OBJECT)

static void
download_cb (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  // PtDownload *self = PT_DOWNLOAD (user_data);
  PtDownload *self = g_object_get_data (source_object, "self");
  g_assert (PT_IS_DOWNLOAD (self));
  GError *error = NULL;

  pt_downloader_download_finish (PT_DOWNLOADER (source_object), res, &error);
  g_signal_emit (self, signals[SIGNAL_FINISHED], 0, error);
  g_timer_stop (self->timer);
}

/**
 * pt_download_get_elapsed_time:
 * @self: self
 *
 * Gets elapsed time in seconds.
 *
 * Returns: time in seconds
 *
 * Since: 5.0
 */
guint64
pt_download_get_elapsed_time (PtDownload *self)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD (self), 0);
  // g_return_val_if_fail (G_IS_TIMER (self->timer), 0);

  return (guint64) g_timer_elapsed (self->timer, NULL);
}

/**
 * pt_download_cancel:
 * @self: self
 *
 * Cancels download.
 *
 * Since: 5.0
 */
void
pt_download_cancel (PtDownload *self)
{
  g_return_if_fail (PT_IS_DOWNLOAD (self));
  g_return_if_fail (G_IS_CANCELLABLE (self->cancellable));

  g_cancellable_cancel (self->cancellable);
}

/**
 * pt_download_start:
 * @self: self
 *
 * Starts download.
 *
 * Since: 5.0
 */
void
pt_download_start (PtDownload *self)
{
  g_return_if_fail (PT_IS_DOWNLOAD (self));

  /* Work around Python plugin not handling user data */
  g_object_set_data (G_OBJECT (self->downloader), "self", self);
  pt_downloader_download_async (self->downloader, self->cancellable, download_cb, self);
  self->timer = g_timer_new ();
}

/**
 * pt_download_get_downloader:
 * @self: self
 *
 * Returns the plugin instance doing the actual download.
 *
 * Returns: (transfer none): plugin instance.
 *
 * Since: 5.0
 */
PtDownloader *
pt_download_get_downloader (PtDownload *self)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD (self), NULL);

  return self->downloader;
}

static void
pt_download_dispose (GObject *object)
{
  PtDownload *self = PT_DOWNLOAD (object);

  peas_engine_unload_plugin (self->engine, self->plugin_info);
  g_clear_object (&self->cancellable);
  g_timer_destroy (self->timer);
  g_free (self->display_name);
  g_free (self->plugin_name);

  G_OBJECT_CLASS (pt_download_parent_class)->dispose (object);
}

static void
pt_download_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  PtDownload *self = PT_DOWNLOAD (object);

  switch (property_id)
    {
    case PROP_DISPLAY_NAME:
      self->display_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_PLUGIN_NAME:
      self->plugin_name = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  PtDownload *self = PT_DOWNLOAD (object);

  switch (property_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, self->display_name);
      break;
    case PROP_PLUGIN_NAME:
      g_value_set_string (value, self->plugin_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_constructed (GObject *object)
{
  PtDownload *self = PT_DOWNLOAD (object);

  self->plugin_info = peas_engine_get_plugin_info (self->engine, self->plugin_name);

  g_assert (PEAS_IS_PLUGIN_INFO (self->plugin_info));

  peas_engine_load_plugin (self->engine, self->plugin_info);
  self->downloader = PT_DOWNLOADER (peas_engine_create_extension (self->engine,
                                                                  self->plugin_info,
                                                                  PT_TYPE_DOWNLOADER,
                                                                  NULL));

  G_OBJECT_CLASS (pt_download_parent_class)->constructed (object);
}

static void
pt_download_init (PtDownload *self)
{
  self->engine = peas_engine_get_default ();
  self->progress = 0.0;
  self->cancellable = g_cancellable_new ();
}

static void
pt_download_class_init (PtDownloadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = pt_download_set_property;
  object_class->get_property = pt_download_get_property;
  object_class->constructed = pt_download_constructed;
  object_class->dispose = pt_download_dispose;

  /**
   * PtDownload::finished:
   * @self: the instance emitting the signal
   * @error: a GError or NULL
   *
   * The #PtDownload::finished signal is emitted after download success or failure.
   * In case of failure, there is en error set; in case of success the error is NULL.
   */
  signals[SIGNAL_FINISHED] = g_signal_new ("finished",
                                           PT_TYPE_DOWNLOAD,
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL,
                                           NULL,
                                           g_cclosure_marshal_VOID__BOXED,
                                           G_TYPE_NONE,
                                           1, G_TYPE_ERROR);

  /**
   * PtDownload:progress:
   *
   * Progress of download between 0.0 and 1.0.
   */
  obj_properties[PROP_PROGRESS] =
      g_param_spec_float (
          "progress", NULL, NULL, 0.0, 1.0,
          0.0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtDownload:display-name:
   *
   * Display name.
   */
  obj_properties[PROP_DISPLAY_NAME] =
      g_param_spec_string (
          "display-name", NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtDownload:plugin-name:
   *
   * Plugin name.
   */
  obj_properties[PROP_PLUGIN_NAME] =
      g_param_spec_string (
          "plugin-name", NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_download_new:
 * @display_name: a user visible name
 * @plugin_name: download plugin's name
 *
 * Creates a new `PtDownload`.
 *
 * Returns: a new #PtDownload
 */
PtDownload *
pt_download_new (char *display_name,
                 char *plugin_name)
{
  g_return_val_if_fail (display_name != NULL, NULL);
  g_return_val_if_fail (plugin_name != NULL, NULL);

  return g_object_new (PT_TYPE_DOWNLOAD,
                       "display-name", display_name,
                       "plugin-name", plugin_name,
                       NULL);
}
