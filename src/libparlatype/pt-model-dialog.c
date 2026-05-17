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
 * PtModelDialog:
 *
 * A dialog for a single ASR configuration.
 *
 * It presents most fields of a `PtConfig`, some of them can be changed, and
 * allows for activating, deactivating or deleting this config.
 */

#include "config.h"

#include "pt-model-dialog.h"

#include "pt-config-item.h"
#include "pt-config-list.h"
#include "pt-config-view.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <parlatype.h>

struct _PtModelDialog
{
  AdwWindow parent_instance;

  PtConfigList *config_list;

  AdwNavigationSplitView *split_view;
  AdwSidebar             *sidebar;
  AdwSidebarSection      *installed_section;
  AdwSidebarSection      *uninstalled_section;

  gulong list_handler;
  gulong download_handler;
};

enum
{
  PROP_0,
  PROP_CONFIG_LIST,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtModelDialog, pt_model_dialog, ADW_TYPE_WINDOW)

static void
installed_cb (PtConfigItem *item,
              GParamSpec   *pspec,
              gpointer      user_data)
{
  PtModelDialog *self = PT_MODEL_DIALOG (user_data);
  if (pt_config_item_is_installed (item))
    {
      adw_sidebar_section_remove (self->uninstalled_section, ADW_SIDEBAR_ITEM (item));
      adw_sidebar_section_append (self->installed_section, ADW_SIDEBAR_ITEM (g_object_ref (item)));
      GListModel *items = adw_sidebar_section_get_items (self->installed_section);
      guint       last_installed_item = g_list_model_get_n_items (items); // item 0 is overview
      adw_sidebar_set_selected (self->sidebar, last_installed_item);
      g_object_unref (items);
    }
  /* FIXME update selection in toolbar, too.
   * Would work with refresh, but deletes all configs.
   * Problematic e.g. if there are several downloads ongoing */
  // pt_config_list_refresh (self->config_list);
}

static void
items_changed_cb (GListModel *list,
                  guint       position, /* bogus */
                  guint       added,    /* bogus */
                  guint       removed,  /* bogus */
                  gpointer    user_data)
{
  /* PtConfigList implements GListModel in a very simplified way.
   * This signal means that everything might have changed. */

  PtModelDialog *self = PT_MODEL_DIALOG (user_data);
  guint          n_configs = g_list_model_get_n_items (G_LIST_MODEL (self->config_list));
  PtConfig      *config;

  adw_sidebar_section_remove_all (self->installed_section);
  adw_sidebar_section_remove_all (self->uninstalled_section);

  for (uint i = 0; i < n_configs; i++)
    {
      config = g_list_model_get_item (G_LIST_MODEL (self->config_list), i);

      PtConfigItem *item = pt_config_item_new (config);

      AdwSidebarSection *section = pt_config_is_installed (config) ? self->installed_section : self->uninstalled_section;
      adw_sidebar_section_append (section, ADW_SIDEBAR_ITEM (item));

      g_signal_connect (item, "notify::installed", G_CALLBACK (installed_cb), self);
      g_object_unref (config);
    }
}

static void
update_content (PtModelDialog *self)
{
  AdwSidebarItem *item = adw_sidebar_get_selected_item (self->sidebar);
  PtConfig       *config;

  if (item == NULL || !PT_IS_CONFIG_ITEM (item))
    {
      GtkWidget *view = adw_toolbar_view_new ();
      GtkWidget *bar = adw_header_bar_new ();
      adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (view), bar);
      AdwNavigationPage *empty_page = adw_navigation_page_new (view, "Title");
      adw_navigation_split_view_set_content (self->split_view, empty_page);
      adw_header_bar_set_show_title (ADW_HEADER_BAR (bar), FALSE);
      return;
    }

  config = pt_config_item_get_config (PT_CONFIG_ITEM (item));
  PtConfigView *child = pt_config_view_new (config, self->config_list);
  adw_navigation_split_view_set_content (self->split_view,
                                         adw_navigation_page_new (GTK_WIDGET (child), "Title"));
}

static void
sidebar_activated_cb (PtModelDialog *self,
                      guint          index)
{
  update_content (self);
  adw_navigation_split_view_set_show_content (self->split_view, TRUE);
}

static void
pt_model_dialog_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  PtModelDialog *self = PT_MODEL_DIALOG (object);

  switch (property_id)
    {
    case PROP_CONFIG_LIST:
      self->config_list = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_model_dialog_dispose (GObject *object)
{
  PtModelDialog *self = PT_MODEL_DIALOG (object);

  g_clear_signal_handler (&self->list_handler, self->config_list);
  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_MODEL_DIALOG);

  G_OBJECT_CLASS (pt_model_dialog_parent_class)->dispose (object);
}

static void
pt_model_dialog_constructed (GObject *object)
{
  PtModelDialog *self = PT_MODEL_DIALOG (object);

  items_changed_cb (G_LIST_MODEL (self->config_list), 0, 1, 0, self);
  self->list_handler = g_signal_connect (self->config_list, "items-changed", G_CALLBACK (items_changed_cb), self);

  G_OBJECT_CLASS (pt_model_dialog_parent_class)->constructed (object);
}

static void
pt_model_dialog_init (PtModelDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  update_content (self);
}

static void
pt_model_dialog_class_init (PtModelDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = pt_model_dialog_set_property;
  object_class->constructed = pt_model_dialog_constructed;
  object_class->dispose = pt_model_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/model-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, PtModelDialog, split_view);
  gtk_widget_class_bind_template_child (widget_class, PtModelDialog, sidebar);
  gtk_widget_class_bind_template_child (widget_class, PtModelDialog, installed_section);
  gtk_widget_class_bind_template_child (widget_class, PtModelDialog, uninstalled_section);
  gtk_widget_class_bind_template_callback (widget_class, sidebar_activated_cb);

  /**
   * PtModelDialog:asr-manager:
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
 * pt_model_dialog_new:
 * @config_list: list of configs
 *
 * Returns a new dialog.
 *
 * Returns: a newly created `PtModelDialog`.
 */
PtModelDialog *
pt_model_dialog_new (PtConfigList *config_list)
{
  return g_object_new (PT_TYPE_MODEL_DIALOG,
                       "config-list", config_list,
                       NULL);
}
