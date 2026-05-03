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

/**
 * PtConfigItem:
 *
 * An action row representing a config.
 */

#include "config.h"

#include "pt-config-item.h"

#include <glib/gi18n.h>

struct _PtConfigItem
{
  AdwSidebarItem parent_instance;

  PtConfig  *config;
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

G_DEFINE_FINAL_TYPE (PtConfigItem, pt_config_item, ADW_TYPE_SIDEBAR_ITEM)

/**
 * pt_config_item_is_installed:
 * self: a `PtConfigItem`
 *
 * Whether the configuration is installed.
 *
 * Returns: true if installed.
 */
gboolean
pt_config_item_is_installed (PtConfigItem *self)
{
  return self->installed;
}

/**
 * pt_config_item_get_active:
 * self: a `PtConfigItem`
 *
 * Whether the configuration is active.
 *
 * Returns: true if active.
 */
gboolean
pt_config_item_get_active (PtConfigItem *self)
{
  return self->active;
}

/**
 * pt_config_item_get_config:
 * self: a `PtConfigItem`
 *
 * Each row is representing a config.
 *
 * Returns: (transfer none): the `PtConfig`.
 */
PtConfig *
pt_config_item_get_config (PtConfigItem *self)
{
  return self->config;
}

/**
 * pt_config_item_set_active:
 * @self: a `PtConfigItem`
 * @active: activate or deactivate
 *
 * Activate or deactivate the config represented by this row.
 */
void
pt_config_item_set_active (PtConfigItem *self,
                           gboolean      active)
{
  if (self->active == active)
    return;

  if (active && !self->installed)
    return;

  self->active = active;
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_ACTIVE]);

  adw_sidebar_item_set_icon_name (ADW_SIDEBAR_ITEM (self), active ? "object-select-symbolic" : NULL);

  /* The NULL image seems to be recognised by screen readers and needs a label.
   * Therefore the image has the role "presentation" and its meaning is
   * transferred to the suffix label. */

  /*gtk_accessible_update_property (GTK_ACCESSIBLE (self->status_label),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  active ? "active" : NULL,
                                  -1);*/
}

static void
config_is_installed_cb (PtConfig   *config,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  PtConfigItem *self = PT_CONFIG_ITEM (user_data);
  self->installed = pt_config_is_installed (config);
}

static void
pt_config_item_constructed (GObject *object)
{
  PtConfigItem *self = PT_CONFIG_ITEM (object);

  g_object_bind_property (
      self->config, "name",
      self, "title",
      G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  adw_sidebar_item_set_subtitle (ADW_SIDEBAR_ITEM (self),
                                 pt_config_get_lang_name (self->config));

  self->installed = pt_config_is_installed (self->config);
  g_signal_connect (
      self->config, "notify::is-installed",
      G_CALLBACK (config_is_installed_cb), self);
}

/**
 * pt_config_item_set_config:
 * @self: a `PtConfigItem`
 * @config: a `PtConfig`
 *
 * Set the config represented by the row. An existing config
 * will be replaced.
 */
void
pt_config_item_set_config (PtConfigItem *self,
                           PtConfig     *config)
{
  g_return_if_fail (PT_IS_CONFIG_ITEM (self));
  g_return_if_fail (PT_IS_CONFIG (config));

  if (config == self->config)
    {
      return;
    }
  g_clear_object (&self->config);
  self->config = g_object_ref (config);
  pt_config_item_constructed (G_OBJECT (self));
}

static void
pt_config_item_dispose (GObject *object)
{
  PtConfigItem *self = PT_CONFIG_ITEM (object);

  g_clear_object (&self->config);

  G_OBJECT_CLASS (pt_config_item_parent_class)->dispose (object);
}

static void
pt_config_item_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PtConfigItem *self = PT_CONFIG_ITEM (object);

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
pt_config_item_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PtConfigItem *self = PT_CONFIG_ITEM (object);

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
pt_config_item_init (PtConfigItem *self)
{
  self->active = FALSE;
  self->installed = FALSE;
}

static void
pt_config_item_class_init (PtConfigItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = pt_config_item_set_property;
  object_class->get_property = pt_config_item_get_property;
  object_class->constructed = pt_config_item_constructed;
  object_class->dispose = pt_config_item_dispose;

  /**
   * PtConfigItem:config:
   *
   * The configuration object.
   */
  obj_properties[PROP_CONFIG] =
      g_param_spec_object (
          "config", NULL, NULL,
          PT_TYPE_CONFIG,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtConfigItem:active:
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
 * pt_config_item_new:
 * @config: configuration data
 *
 * Returns a new PtConfigItem instance for the config.
 *
 * After use g_object_unref() it.
 *
 * Returns: (transfer full): a new #PtConfigItem
 */
PtConfigItem *
pt_config_item_new (PtConfig *config)
{
  g_return_val_if_fail (config != NULL, NULL);
  g_return_val_if_fail (PT_IS_CONFIG (config), NULL);

  return g_object_new (PT_TYPE_CONFIG_ITEM,
                       "config", config,
                       NULL);
}
