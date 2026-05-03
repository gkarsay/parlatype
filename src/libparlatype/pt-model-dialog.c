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

  PtConfig          *config;
  GSettings         *editor;
  PtAsrManager      *asr_manager;
  PtDownloadManager *download_manager;
  PtConfigList      *list;

  AdwSidebar             *sidebar;
  AdwNavigationSplitView *split_view;
  GtkWidget              *installed_section;
  GtkWidget              *uninstalled_section;
};

enum
{
  PROP_0,
  PROP_ASR_MANAGER,
  PROP_DOWNLOAD_MANAGER,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtModelDialog, pt_model_dialog, ADW_TYPE_WINDOW)

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
  guint          n_configs = g_list_model_get_n_items (G_LIST_MODEL (self->list));
  PtConfig      *config;
  PtConfig      *active_config;
  gboolean       has_config;

  active_config = pt_asr_manager_get_config (self->asr_manager);
  has_config = active_config != NULL;

  for (uint i = 0; i < n_configs; i++)
    {
      config = g_list_model_get_item (G_LIST_MODEL (self->list), i);

      PtConfigItem *item = pt_config_item_new (config);
      if (has_config && pt_config_equal (config, active_config))
        pt_config_item_set_active (item, TRUE);

      AdwSidebarSection *section = pt_config_is_installed (config) ? ADW_SIDEBAR_SECTION (self->installed_section) : ADW_SIDEBAR_SECTION (self->uninstalled_section);
      adw_sidebar_section_append (section, ADW_SIDEBAR_ITEM (item));

      g_object_unref (config);
    }
}

static void
update_content (PtModelDialog *self)
{
  AdwSidebarItem *item = adw_sidebar_get_selected_item (self->sidebar);
  PtConfig       *config;
  /*GType type;
  GtkWidget *child;
  const char *title;

  if (self->active_item == item)
    return;

  self->active_item = item;*/

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
  PtConfigView *child = pt_config_view_new (config, self->asr_manager, self->download_manager);
  adw_navigation_split_view_set_content (self->split_view,
                                         adw_navigation_page_new (GTK_WIDGET (child), "Title"));

  /*g_assert (ADW_IS_DEMO_SIDEBAR_ITEM (item));

  type = adw_demo_sidebar_item_get_page_type (ADW_DEMO_SIDEBAR_ITEM (item));
  child = g_object_new (type, NULL);
  title = adw_sidebar_item_get_title (item);

  adw_navigation_split_view_set_content (self->split_view,
                                         adw_navigation_page_new (child, title));*/
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
    case PROP_ASR_MANAGER:
      self->asr_manager = g_value_get_object (value);
      break;
    case PROP_DOWNLOAD_MANAGER:
      self->download_manager = g_value_get_object (value);
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

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_MODEL_DIALOG);

  G_OBJECT_CLASS (pt_model_dialog_parent_class)->dispose (object);
}

static void
pt_model_dialog_constructed (GObject *object)
{
  PtModelDialog *self = PT_MODEL_DIALOG (object);

  self->list = pt_config_list_new (self->asr_manager);
  g_signal_connect (self->list, "items-changed", G_CALLBACK (items_changed_cb), self);

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
  obj_properties[PROP_ASR_MANAGER] =
      g_param_spec_object (
          "asr-manager", NULL, NULL,
          PT_TYPE_ASR_MANAGER,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * PtModelDialog:download-manager:
   *
   * A reference on a PtPlayer is needed, so that ASR plugins get audio input.
   */
  obj_properties[PROP_DOWNLOAD_MANAGER] =
      g_param_spec_object (
          "download-manager", NULL, NULL,
          PT_TYPE_DOWNLOAD_MANAGER,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_model_dialog_new:
 * @asr_manager: xxx
 * @download_manager: xxx
 *
 * Returns a new dialog. Next set a config with pt_model_dialog_set_config().
 *
 * Returns: a newly created `PtModelDialog`.
 */
PtModelDialog *
pt_model_dialog_new (PtAsrManager      *asr_manager,
                     PtDownloadManager *download_manager)
{
  return g_object_new (PT_TYPE_MODEL_DIALOG,
                       "asr-manager", asr_manager,
                       "download-manager", download_manager,
                       NULL);
}
