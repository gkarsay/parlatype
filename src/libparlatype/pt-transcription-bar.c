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
 * PtTranscriptionBar:
 *
 * Presents widgets for automatic transcription.
 *
 * `PtTranscriptionBar` has a revealer and contains widgets related to automatic transcription.
 *
 * - A drop-down to select a model (presenting installed models)
 * - A split-button to start/stop automatic transcription. Its menu is populated with options for transcription supported by the plugin used.
 * - A button to access the model dialog.
 * - A button to close the bar.
 *
 * Since: 5.0
 */

#define G_LOG_DOMAIN "pt-transcription-bar"

#include "config.h"

#include "pt-transcription-bar.h"

#include "pt-config-list.h"
#include "pt-config-private.h"
#include "pt-config.h"

#include <glib/gi18n.h>

struct _PtTranscriptionBar
{
  GtkWidget parent_instance;

  GtkWidget   *revealer;
  GtkWidget   *box;
  GtkDropDown *model_selector;
  GtkWidget   *transcribe_button;
  GtkWidget   *options_box;

  PtConfigList       *config_list;
  GtkFilterListModel *filtered_list;
  PtConfig           *config;
};

enum
{
  PROP_0,
  PROP_REVEALED,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_FINAL_TYPE (PtTranscriptionBar, pt_transcription_bar, GTK_TYPE_WIDGET)

/**
 * pt_transcription_bar_get_revealed:
 * @self: a transcription bar
 *
 * Gets whether the bar is revealed.
 *
 * Returns: True for a revealed bar.
 *
 * Since: 5.0
 */
gboolean
pt_transcription_bar_get_revealed (PtTranscriptionBar *self)
{
  g_return_val_if_fail (PT_IS_TRANSCRIPTION_BAR (self), FALSE);

  return gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer));
}

/**
 * pt_transcription_bar_set_revealed:
 * @self: a transcription bar
 * @revealed: reveal status
 *
 * Sets whether the transcription bar should be revealed (shown) or not.
 *
 * Since: 5.0
 */
void
pt_transcription_bar_set_revealed (PtTranscriptionBar *self,
                                   gboolean            revealed)
{
  g_return_if_fail (PT_IS_TRANSCRIPTION_BAR (self));

  if (gtk_revealer_get_reveal_child (GTK_REVEALER (self->revealer)) == revealed)
    return;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), revealed);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_REVEALED]);
}

/**
 * pt_transcription_bar_get_options_box:
 * @self: a transcription bar
 *
 * Gets options box, where plugins can insert options.
 *
 * Returns: (transfer none): an options box.
 *
 * Since: 5.0
 */
GtkWidget *
pt_transcription_bar_get_options_box (PtTranscriptionBar *self)
{
  g_return_val_if_fail (PT_IS_TRANSCRIPTION_BAR (self), NULL);

  return self->options_box;
}

static void
model_selector_changed (GtkDropDown *drop_down,
                        GParamSpec  *pspec,
                        gpointer     user_data)
{
  PtTranscriptionBar *self = PT_TRANSCRIPTION_BAR (user_data);

  guint selected = gtk_drop_down_get_selected (drop_down);
  if (selected == GTK_INVALID_LIST_POSITION)
    return;

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->filtered_list)); i++)
    {
      PtConfig *item = g_list_model_get_item (G_LIST_MODEL (self->filtered_list), i);
      _pt_config_set_active (item, i == selected);
      if (i == selected)
        pt_config_list_set_active (self->config_list, item);
    }
}

/**
 * pt_transcription_bar_set_config_list:
 * @self: a transcription bar
 * @config_list: a config list.
 *
 * Sets the active config list that is used in the drop-down to present available models.
 *
 * Since: 5.0
 */
void
pt_transcription_bar_set_config_list (PtTranscriptionBar *self,
                                      PtConfigList       *config_list)
{
  g_return_if_fail (PT_IS_TRANSCRIPTION_BAR (self));
  g_return_if_fail (PT_IS_CONFIG_LIST (config_list));
  g_return_if_fail (self->config_list == NULL);

  /* Create filtered  model for installed models only */
  GtkExpression *filter_expr = gtk_property_expression_new (PT_TYPE_CONFIG, NULL, "is-installed");
  GtkBoolFilter *filter = gtk_bool_filter_new (filter_expr);
  self->config_list = config_list;
  self->filtered_list = gtk_filter_list_model_new (G_LIST_MODEL (config_list), GTK_FILTER (filter));
  gtk_filter_list_model_set_watch_items (self->filtered_list, TRUE);

  GtkExpression *expression = gtk_property_expression_new (PT_TYPE_CONFIG, NULL, "name");
  gtk_drop_down_set_expression (self->model_selector, expression);
  gtk_drop_down_set_enable_search (self->model_selector, FALSE);
  gtk_drop_down_set_model (self->model_selector, G_LIST_MODEL (self->filtered_list));
  g_signal_connect (self->model_selector, "notify::selected", G_CALLBACK (model_selector_changed), self);

  /* Select active item */
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->filtered_list)); i++)
    {
      PtConfig *item = g_list_model_get_item (G_LIST_MODEL (self->filtered_list), i);
      if (pt_config_is_active (item))
        {
          gtk_drop_down_set_selected (self->model_selector, i);
          break;
        }
    }
}

static void
close_button_clicked (PtTranscriptionBar *self)
{
  g_assert (PT_IS_TRANSCRIPTION_BAR (self));

  pt_transcription_bar_set_revealed (self, FALSE);
}

static void
pt_transcription_bar_compute_expand (GtkWidget *widget,
                                     gboolean  *hexpand,
                                     gboolean  *vexpand)
{
  PtTranscriptionBar *self = PT_TRANSCRIPTION_BAR (widget);
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
}

static void
pt_transcription_bar_dispose (GObject *object)
{
  PtTranscriptionBar *self = PT_TRANSCRIPTION_BAR (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_TRANSCRIPTION_BAR);
  g_clear_object (&self->filtered_list);

  G_OBJECT_CLASS (pt_transcription_bar_parent_class)->dispose (object);
}

static void
pt_transcription_bar_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  PtTranscriptionBar *self = PT_TRANSCRIPTION_BAR (object);

  switch (property_id)
    {
    case PROP_REVEALED:
      pt_transcription_bar_set_revealed (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_transcription_bar_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  PtTranscriptionBar *self = PT_TRANSCRIPTION_BAR (object);

  switch (property_id)
    {
    case PROP_REVEALED:
      g_value_set_boolean (value, pt_transcription_bar_get_revealed (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_transcription_bar_init (PtTranscriptionBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_transcription_bar_class_init (PtTranscriptionBarClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = pt_transcription_bar_set_property;
  object_class->get_property = pt_transcription_bar_get_property;
  object_class->dispose = pt_transcription_bar_dispose;

  widget_class->compute_expand = pt_transcription_bar_compute_expand;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/transcription-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, box);
  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, revealer);
  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, model_selector);
  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, transcribe_button);
  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, options_box);

  gtk_widget_class_bind_template_callback (widget_class, close_button_clicked);

  /* borrow style from GtkSearchBar */
  gtk_widget_class_set_css_name (widget_class, "searchbar");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);

  /**
   * PtTranscriptionBar:revealed:
   *
   * Whether the bar is revealed (shown) or not.
   */
  obj_properties[PROP_REVEALED] =
      g_param_spec_boolean (
          "revealed", NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

/**
 * pt_transcription_bar_new:
 *
 * Creates a new `PtTranscriptionBar`.
 *
 * Returns: a new #PtTranscriptionBar
 */
PtTranscriptionBar *
pt_transcription_bar_new (void)
{
  return g_object_new (PT_TYPE_TRANSCRIPTION_BAR, NULL);
}
