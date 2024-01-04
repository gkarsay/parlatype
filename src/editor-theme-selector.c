/* editor-theme-selector.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

//#define G_LOG_DOMAIN "editor-theme-selector"

#include "config.h"

#include <adwaita.h>

#include "editor-theme-selector-private.h"

struct _EditorThemeSelector
{
  GtkWidget parent_instance;
  GtkWidget *box;
  GtkToggleButton *dark;
  GtkToggleButton *light;
  GtkToggleButton *follow;
  char *theme;
};

G_DEFINE_TYPE (EditorThemeSelector, editor_theme_selector, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_THEME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GtkWidget *
_editor_theme_selector_new (void)
{
  return g_object_new (EDITOR_TYPE_THEME_SELECTOR, NULL);
}

static void
on_notify_system_supports_color_schemes_cb (EditorThemeSelector *self,
                                            GParamSpec          *pspec,
                                            AdwStyleManager     *style_manager)
{
  gboolean visible;

  g_assert (EDITOR_IS_THEME_SELECTOR (self));
  g_assert (ADW_IS_STYLE_MANAGER (style_manager));

  visible = adw_style_manager_get_system_supports_color_schemes (style_manager);
  gtk_widget_set_visible (GTK_WIDGET (self->follow), visible);
}

static void
on_notify_dark_cb (EditorThemeSelector *self,
                   GParamSpec          *pspec,
                   AdwStyleManager     *style_manager)
{
  g_assert (EDITOR_IS_THEME_SELECTOR (self));
  g_assert (ADW_IS_STYLE_MANAGER (style_manager));

  style_manager = adw_style_manager_get_default ();

  if (adw_style_manager_get_dark (style_manager))
    gtk_widget_add_css_class (GTK_WIDGET (self), "dark");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "dark");
}

static void
editor_theme_selector_dispose (GObject *object)
{
  EditorThemeSelector *self = (EditorThemeSelector *)object;

  g_clear_pointer (&self->box, gtk_widget_unparent);
  g_clear_pointer (&self->theme, g_free);

  G_OBJECT_CLASS (editor_theme_selector_parent_class)->dispose (object);
}

static void
editor_theme_selector_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EditorThemeSelector *self = EDITOR_THEME_SELECTOR (object);

  switch (prop_id)
    {
    case PROP_THEME:
      g_value_set_string (value, _editor_theme_selector_get_theme (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_theme_selector_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EditorThemeSelector *self = EDITOR_THEME_SELECTOR (object);

  switch (prop_id)
    {
    case PROP_THEME:
      _editor_theme_selector_set_theme (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_theme_selector_class_init (EditorThemeSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = editor_theme_selector_dispose;
  object_class->get_property = editor_theme_selector_get_property;
  object_class->set_property = editor_theme_selector_set_property;

  properties [PROP_THEME] =
    g_param_spec_string ("theme",
                         "Theme",
                         "Theme",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "themeselector");
  gtk_widget_class_install_property_action (widget_class, "theme.mode", "theme");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/Parlatype/editor-theme-selector.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, EditorThemeSelector, box);
  gtk_widget_class_bind_template_child (widget_class, EditorThemeSelector, dark);
  gtk_widget_class_bind_template_child (widget_class, EditorThemeSelector, light);
  gtk_widget_class_bind_template_child (widget_class, EditorThemeSelector, follow);
}

static void
editor_theme_selector_init (EditorThemeSelector *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  gboolean dark;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (style_manager,
                           "notify::system-supports-color-schemes",
                           G_CALLBACK (on_notify_system_supports_color_schemes_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (style_manager,
                           "notify::dark",
                           G_CALLBACK (on_notify_dark_cb),
                           self,
                           G_CONNECT_SWAPPED);

  dark = adw_style_manager_get_dark (style_manager);
  self->theme = g_strdup (dark ? "dark" : "light");

  on_notify_system_supports_color_schemes_cb (self, NULL, style_manager);
  on_notify_dark_cb (self, NULL, style_manager);
}

const char *
_editor_theme_selector_get_theme (EditorThemeSelector *self)
{
  g_return_val_if_fail (EDITOR_IS_THEME_SELECTOR (self), NULL);

  return self->theme;
}

void
_editor_theme_selector_set_theme (EditorThemeSelector *self,
                                  const char          *theme)
{
  g_return_if_fail (EDITOR_IS_THEME_SELECTOR (self));

  if (g_strcmp0 (theme, self->theme) != 0)
    {
      g_free (self->theme);
      self->theme = g_strdup (theme);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_THEME]);
    }
}
