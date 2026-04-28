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
 * Manages things.
 *
 * Still in development.
 *
 * Since: 5.0
 */

#define G_LOG_DOMAIN "pt-transcription-bar"

#include "config.h"

#include "pt-transcription-bar.h"

#include <glib/gi18n.h>

struct _PtTranscriptionBar
{
  GtkWidget parent_instance;

  GtkWidget *revealer;
  GtkWidget *box;
  GtkWidget *model_button;
  GtkWidget *close_button;

  PtConfig *config;
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
 * Gets if a banner is revealed
 *
 * Returns: Whether a banner is revealed
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
 * @self: a banner
 * @revealed: whether a banner should be revealed
 *
 * Sets whether a banner should be revealed
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
 * pt_transcription_bar_get_config:
 * @self: a transcription bar
 *
 * Gets the active config.
 *
 * Returns: (transfer none): a `PtConfig`
 *
 * Since: 5.0
 */
PtConfig *
pt_transcription_bar_get_config (PtTranscriptionBar *self)
{
  g_return_val_if_fail (PT_IS_TRANSCRIPTION_BAR (self), FALSE);

  return self->config;
}

/**
 * pt_transcription_bar_set_config:
 * @self: a transcription bar
 * @config: a config
 *
 * Sets the active config.
 *
 * Since: 5.0
 */
void
pt_transcription_bar_set_config (PtTranscriptionBar *self,
                                 PtConfig           *config)
{
  g_return_if_fail (PT_IS_TRANSCRIPTION_BAR (self));
  g_return_if_fail (PT_IS_CONFIG (config) || config == NULL);

  if (self->config == config)
    return;

  if (self->config == NULL)
    {
      gtk_button_set_label (GTK_BUTTON (self->model_button), _ ("Choose Model"));
      g_clear_object (&self->config);
    }
  else
    {
      gtk_button_set_label (GTK_BUTTON (self->model_button), pt_config_get_name (config));
      self->config = g_object_ref (config);
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
pt_transcription_bar_constructed (GObject *object)
{
  // PtTranscriptionBar *self = PT_TRANSCRIPTION_BAR (object);

  G_OBJECT_CLASS (pt_transcription_bar_parent_class)->constructed (object);
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
  object_class->constructed = pt_transcription_bar_constructed;
  object_class->dispose = pt_transcription_bar_dispose;

  widget_class->compute_expand = pt_transcription_bar_compute_expand;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/transcription-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, box);
  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, revealer);
  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, model_button);
  gtk_widget_class_bind_template_child (widget_class, PtTranscriptionBar, close_button);

  gtk_widget_class_bind_template_callback (widget_class, close_button_clicked);

  /* borrow style from GtkSearchBar */
  gtk_widget_class_set_css_name (widget_class, "searchbar");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);

  /**
   * PtTranscriptionBar:revealed:
   *
   * Describe me.
   */
  obj_properties[PROP_REVEALED] =
      g_param_spec_boolean (
          "revealed", NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
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
