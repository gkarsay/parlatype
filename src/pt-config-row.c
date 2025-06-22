/* Copyright 2020 Gabor Karsay <gabor.karsay@gmx.at>
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

#include "pt-config-row.h"

#include <glib/gi18n.h>

struct _PtConfigRow
{
  AdwActionRow parent;

  PtConfig  *config;
  GtkWidget *status_label;
  GtkWidget *active_image;
  gboolean   active;
  gboolean   installed;
};

enum
{
  PROP_CONFIG = 1,
  PROP_ACTIVE,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtConfigRow, pt_config_row, ADW_TYPE_ACTION_ROW)

/**
 * SECTION: pt-config-row
 * @short_description:
 * @stability: Unstable
 * @include: parlatype/pt-config-row.h
 *
 * TODO
 */

gboolean
pt_config_row_is_installed (PtConfigRow *self)
{
  return self->installed;
}

gboolean
pt_config_row_get_active (PtConfigRow *self)
{
  return self->active;
}

PtConfig *
pt_config_row_get_config (PtConfigRow *self)
{
  return self->config;
}

static void
set_status_label (PtConfigRow *self)
{
  GtkLabel *label = GTK_LABEL (self->status_label);
  gchar    *status = NULL;

  if (!self->installed)
    status = _ ("Not installed");

  gtk_label_set_label (label, status);
}

void
pt_config_row_set_active (PtConfigRow *self,
                          gboolean     active)
{
  if (self->active == active)
    return;

  if (active && !self->installed)
    return;

  self->active = active;
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_ACTIVE]);

  gtk_image_set_from_icon_name (GTK_IMAGE (self->active_image),
                                active ? "object-select-symbolic" : NULL);

  /* The NULL image seems to be recognised by screen readers and needs a label.
   * Therefore the image has the role "presentation" and its meaning is
   * transferred to the suffix label. */

  gtk_accessible_update_property (GTK_ACCESSIBLE (self->status_label),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  active ? "active" : NULL,
                                  -1);
}

static void
config_is_installed_cb (PtConfig   *config,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  PtConfigRow *self = PT_CONFIG_ROW (user_data);
  self->installed = pt_config_is_installed (config);
  set_status_label (self);
}

static void
pt_config_row_constructed (GObject *object)
{
  PtConfigRow *self = PT_CONFIG_ROW (object);

  g_object_bind_property (
      self->config, "name",
      self, "title",
      G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self),
                               pt_config_get_lang_name (self->config));

  self->installed = pt_config_is_installed (self->config);
  g_signal_connect (
      self->config, "notify::is-installed",
      G_CALLBACK (config_is_installed_cb), self);

  set_status_label (self);
}

void
pt_config_row_set_config (PtConfigRow *self,
                          PtConfig    *config)
{
  g_return_if_fail (PT_IS_CONFIG_ROW (self));
  g_return_if_fail (PT_IS_CONFIG (config));

  if (config == self->config)
    {
      return;
    }
  g_clear_object (&self->config);
  self->config = g_object_ref (config);
  pt_config_row_constructed (G_OBJECT (self));
}

static void
pt_config_row_init (PtConfigRow *self)
{
  self->active = FALSE;
  self->installed = FALSE;

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_config_row_dispose (GObject *object)
{
  PtConfigRow *self = PT_CONFIG_ROW (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_CONFIG_ROW);

  G_OBJECT_CLASS (pt_config_row_parent_class)->dispose (object);
}

static void
pt_config_row_finalize (GObject *object)
{
  PtConfigRow *self = PT_CONFIG_ROW (object);

  g_clear_object (&self->config);

  G_OBJECT_CLASS (pt_config_row_parent_class)->finalize (object);
}

static void
pt_config_row_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  PtConfigRow *self = PT_CONFIG_ROW (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      g_clear_object (&self->config);
      self->config = g_value_dup_object (value);
      break;
    case PROP_ACTIVE:
      self->active = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_config_row_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PtConfigRow *self = PT_CONFIG_ROW (object);

  switch (property_id)
    {
    case PROP_CONFIG:
      g_value_set_object (value, self->config);
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_config_row_class_init (PtConfigRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = pt_config_row_set_property;
  object_class->get_property = pt_config_row_get_property;
  object_class->constructed = pt_config_row_constructed;
  object_class->dispose = pt_config_row_dispose;
  object_class->finalize = pt_config_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/config-row.ui");
  gtk_widget_class_bind_template_child (widget_class, PtConfigRow, status_label);
  gtk_widget_class_bind_template_child (widget_class, PtConfigRow, active_image);

  /**
   * PtConfigRow:config:
   *
   * The configuration object.
   */
  obj_properties[PROP_CONFIG] =
      g_param_spec_object (
          "config", NULL, NULL,
          PT_TYPE_CONFIG,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfigRow:active:
   *
   * Whether the configuration is active.
   */
  obj_properties[PROP_ACTIVE] =
      g_param_spec_boolean (
          "active", NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_config_row_new:
 * @row_file: path to the file with the settings
 *
 * Returns a new PtConfigRow instance for the given file. If the file
 * doesn’t exist, it will be created.
 *
 * After use g_object_unref() it.
 *
 * Return value: (transfer full): a new #PtConfigRow
 */
PtConfigRow *
pt_config_row_new (PtConfig *config)
{
  g_return_val_if_fail (config != NULL, NULL);
  g_return_val_if_fail (PT_IS_CONFIG (config), NULL);

  return g_object_new (PT_TYPE_CONFIG_ROW,
                       "config", config,
                       NULL);
}
