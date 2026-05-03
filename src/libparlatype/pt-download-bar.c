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
 * PtDownloadBar:
 *
 * Shows download progress.
 *
 * Before download set the name (model name that is to be downloaded),
 * the total number of bytes and a cancellable.
 *
 * During download set the current number of downloaded bytes.
 *
 * Since: 5.0
 */

#define G_LOG_DOMAIN "pt-download-bar"

#include "config.h"

#include "pt-download-bar.h"

#include <glib/gi18n.h>

struct _PtDownloadBar
{
  GtkWidget parent_instance;

  GtkWidget *revealer;
  GtkWidget *start_label;
  GtkWidget *progress_bar;
  GtkWidget *cancel_button;

  char         *name;
  GCancellable *cancellable;
  int           current, total;
};

enum
{
  PROP_0,
  PROP_REVEALED,
  PROP_NAME,
  PROP_CURRENT,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtDownloadBar, pt_download_bar, GTK_TYPE_WIDGET)

/**
 * pt_download_bar_get_revealed:
 * @self: a download bar
 *
 * Gets if the bar is revealed
 *
 * Returns: Whether a banner is revealed
 *
 * Since: 5.0
 */
gboolean
pt_download_bar_get_revealed (PtDownloadBar *self)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD_BAR (self), FALSE);

  return gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer));
}

/**
 * pt_download_bar_set_revealed:
 * @self: a download bar
 * @revealed: whether the bar should be revealed
 *
 * Sets whether the bar should be revealed.
 *
 * Since: 5.0
 */
void
pt_download_bar_set_revealed (PtDownloadBar *self,
                              gboolean       revealed)
{
  g_return_if_fail (PT_IS_DOWNLOAD_BAR (self));

  if (gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer)) == revealed)
    return;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), revealed);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_REVEALED]);
}

/**
 * pt_download_bar_get_name:
 * @self: a download bar
 *
 * Gets the display name for the current download.
 *
 * Returns: (transfer none): a download name
 *
 * Since: 5.0
 */
char *
pt_download_bar_get_name (PtDownloadBar *self)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD_BAR (self), FALSE);

  return self->name;
}

/**
 * pt_download_bar_set_name:
 * @self: a download bar
 * @name: name of download item
 *
 * Sets the name of the download item.
 *
 * Since: 5.0
 */
void
pt_download_bar_set_name (PtDownloadBar *self,
                          char          *name)
{
  g_return_if_fail (PT_IS_DOWNLOAD_BAR (self));
  g_return_if_fail (name != NULL);

  if (g_strcmp0 (self->name, name) == 0)
    return;

  g_free (self->name);
  self->name = g_strdup (name);

  gtk_label_set_text (GTK_LABEL (self->start_label), name);
}

/**
 * pt_download_bar_get_current:
 * @self: a download bar
 *
 * Gets the current number of downloaded bytes.
 *
 * Returns: number of current bytes.
 *
 * Since: 5.0
 */
int
pt_download_bar_get_current (PtDownloadBar *self)
{
  g_return_val_if_fail (PT_IS_DOWNLOAD_BAR (self), FALSE);

  return self->current;
}

/**
 * pt_download_bar_set_current:
 * @self: a download bar
 * @current: number of already downloaded bytes
 *
 * Sets the number of already downloaded bytes.
 *
 * Since: 5.0
 */
void
pt_download_bar_set_current (PtDownloadBar *self,
                             int            current)
{
  double fraction;

  g_return_if_fail (PT_IS_DOWNLOAD_BAR (self));
  g_return_if_fail (current >= 0 && current <= self->total);

  if (self->current == current)
    return;

  fraction = current / (double) self->total;
  // g_print ("%f\n", fraction);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), fraction);
}

/**
 * pt_download_bar_set_total:
 * @self: a download bar
 * @total: total size of download in bytes
 *
 * Sets total size of download in bytes.
 *
 * Since: 5.0
 */
void
pt_download_bar_set_total (PtDownloadBar *self,
                           int            total)
{
  g_return_if_fail (PT_IS_DOWNLOAD_BAR (self));
  g_return_if_fail (total >= 0);

  if (self->total == total)
    return;

  self->total = total;
  g_print ("total: %d\n", self->total);
}

/**
 * pt_download_bar_set_cancellable:
 * @self: a download bar
 * @cancellable: a cancellable
 *
 * Sets the cancellabe so that the user can cancel the operation.
 *
 * Since: 5.0
 */
void
pt_download_bar_set_cancellable (PtDownloadBar *self,
                                 GCancellable  *cancellable)
{
  g_return_if_fail (PT_IS_DOWNLOAD_BAR (self));
  g_return_if_fail (G_IS_CANCELLABLE (cancellable));

  self->cancellable = cancellable;
}

static void
cancel_button_clicked (PtDownloadBar *self)
{
  g_assert (PT_IS_DOWNLOAD_BAR (self));
  g_assert (G_IS_CANCELLABLE (self->cancellable));

  g_cancellable_cancel (self->cancellable);

  pt_download_bar_set_revealed (self, FALSE);
}

/*static void
pt_download_bar_compute_expand (GtkWidget *widget,
                                gboolean  *hexpand,
                                gboolean  *vexpand)
{
  PtDownloadBar *self = PT_DOWNLOAD_BAR (widget);
  if (self->box)
    {
      *hexpand = gtk_widget_compute_expand (self->box, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (self->box, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}*/

static void
pt_download_bar_dispose (GObject *object)
{
  PtDownloadBar *self = PT_DOWNLOAD_BAR (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_DOWNLOAD_BAR);
  g_free (self->name);

  G_OBJECT_CLASS (pt_download_bar_parent_class)->dispose (object);
}

static void
pt_download_bar_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PtDownloadBar *self = PT_DOWNLOAD_BAR (object);

  switch (property_id)
    {
    case PROP_REVEALED:
      pt_download_bar_set_revealed (self, g_value_get_boolean (value));
      break;
    case PROP_NAME:
      pt_download_bar_set_name (self, (void *) g_value_get_string (value));
      break;
    case PROP_CURRENT:
      pt_download_bar_set_current (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_bar_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  PtDownloadBar *self = PT_DOWNLOAD_BAR (object);

  switch (property_id)
    {
    case PROP_REVEALED:
      g_value_set_boolean (value, pt_download_bar_get_revealed (self));
      break;
    case PROP_NAME:
      g_value_set_string (value, pt_download_bar_get_name (self));
      break;
    case PROP_CURRENT:
      g_value_set_int (value, pt_download_bar_get_current (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_download_bar_init (PtDownloadBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_download_bar_class_init (PtDownloadBarClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = pt_download_bar_set_property;
  object_class->get_property = pt_download_bar_get_property;
  object_class->dispose = pt_download_bar_dispose;

  // widget_class->compute_expand = pt_download_bar_compute_expand;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/download-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, PtDownloadBar, revealer);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadBar, start_label);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadBar, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, PtDownloadBar, cancel_button);

  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked);

  /* borrow style from GtkSearchBar */
  gtk_widget_class_set_css_name (widget_class, "searchbar");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);

  /**
   * PtDownloadBar:revealed:
   *
   * Describe me.
   */
  obj_properties[PROP_REVEALED] =
      g_param_spec_boolean (
          "revealed", NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * PtDownloadBar:name:
   *
   * Describe me.
   */
  obj_properties[PROP_NAME] =
      g_param_spec_string (
          "name", NULL, NULL,
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * PtDownloadBar:current:
   *
   * Describe me.
   */
  obj_properties[PROP_CURRENT] =
      g_param_spec_int (
          "current", NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

/**
 * pt_download_bar_new:
 *
 * Creates a new `PtDownloadBar`.
 *
 * Returns: a new #PtDownloadBar
 */
PtDownloadBar *
pt_download_bar_new (void)
{
  return g_object_new (PT_TYPE_DOWNLOAD_BAR, NULL);
}
