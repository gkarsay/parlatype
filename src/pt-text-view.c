/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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

#include "pt-text-view.h"

#include "pt-text-buffer.h"
#include "pt-window-private.h"

struct _PtTextView
{
  GtkTextView parent;

  PtTextBuffer         *buffer;
  GtkTextTag           *timestamp_tag;
  GtkCssProvider       *css_provider;
  int                   font_size;
  PangoFontDescription *font_desc;

  PtPlayer  *player;
  GSettings *settings;
};

G_DEFINE_FINAL_TYPE (PtTextView, pt_text_view, GTK_TYPE_TEXT_VIEW)

static char *
get_timestamp_at_iter (PtTextView *self,
                       GtkTextIter iter)
{
  GtkTextIter start, end;
  char       *timestamp = NULL;

  start = end = iter;
  if (gtk_text_iter_backward_to_tag_toggle (&start, self->timestamp_tag) &&
      gtk_text_iter_forward_to_tag_toggle (&end, self->timestamp_tag))
    {
      timestamp = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (self->buffer), &start, &end, FALSE);
    }

  return timestamp;
}

static void
goto_timestamp_at_iter (PtTextView *self,
                        GtkTextIter iter)
{
  char *timestamp = get_timestamp_at_iter (self, iter);
  if (timestamp != NULL)
    {
      pt_player_goto_timestamp (self->player, timestamp);
      g_free (timestamp);
    }
}

static gboolean
key_pressed (GtkEventController *controller,
             guint               keyval,
             guint               keycode,
             GdkModifierType     modifiers,
             PtTextView         *self)
{
  GtkTextIter         iter;
  GtkTextChildAnchor *anchor;
  guint               out_len;

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), &iter,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer)));

  switch (keyval)
    {
    case GDK_KEY_Left:
      g_print ("left\n");
      gtk_text_iter_backward_char (&iter);
      anchor = gtk_text_iter_get_child_anchor (&iter);
      if (anchor != NULL && gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (self)))
        {
          g_print ("anchor\n");
          gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self), FALSE);
          gtk_text_view_set_editable (GTK_TEXT_VIEW (self), FALSE);
          GtkWidget **list = gtk_text_child_anchor_get_widgets (anchor, &out_len);
          gtk_widget_grab_focus (list[0]);
        }
      else
        {
          if (!gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (self)))
            {
              gtk_widget_grab_focus (GTK_WIDGET (self));
              gtk_text_view_set_editable (GTK_TEXT_VIEW (self), TRUE);
              gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self), TRUE);
            }
        }
      break;
    case GDK_KEY_Right:
      g_print ("right\n");
      anchor = gtk_text_iter_get_child_anchor (&iter);
      g_print ("anchor : %s\n", (anchor == NULL) ? "no" : "yes");
      g_print ("visible: %s\n", (gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (self))) ? "yes" : "no");
      if (anchor != NULL && gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (self)))
        {
          g_print ("anchor\n");
          gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self), FALSE);
          gtk_text_view_set_editable (GTK_TEXT_VIEW (self), FALSE);
          GtkWidget **list = gtk_text_child_anchor_get_widgets (anchor, &out_len);
          gtk_widget_grab_focus (list[0]);
        }
      else
        {
          if (!gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (self)))
            {
              gtk_widget_grab_focus (GTK_WIDGET (self));
              gtk_text_view_set_editable (GTK_TEXT_VIEW (self), TRUE);
              gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (self), TRUE);
            }
        }
      break;

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
pt_text_view_update_css (PtTextView *self)
{
  char *css = g_strdup_printf ("textview {font-family: %s; font-size: %dpt;} "
                               "textview button {font-size: calc(%dpt * 0.8); padding: 0.3em; margin-bottom: -0.3em; margin-left: 0.5em; margin-right: 0.5em;}",
                               pango_font_description_get_family (self->font_desc),
                               self->font_size,
                               self->font_size);
  gtk_css_provider_load_from_string (self->css_provider, css);
  g_free (css);

  /* Update width to be roughly 80 latin n characters.
   * This is not pixel perfect, size request and actual size can differ. */

  GString       *width_string;
  PangoLayout   *layout;
  PangoRectangle rect;

  width_string = g_string_new ("");
  for (int i = 0; i < 84; i++)
    {
      g_string_append (width_string, "n");
    }

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), width_string->str);
  pango_layout_get_pixel_extents (layout, &rect, NULL);
  gtk_widget_set_size_request (GTK_WIDGET (self), rect.width, -1);

  g_string_free (width_string, TRUE);
  g_object_unref (layout);
}

static void
pt_text_view_zoom (PtTextView *self,
                   int         amount)
{
  if (amount == 0)
    {
      g_settings_reset (self->settings, "font-size");
      self->font_size = g_settings_get_int (self->settings, "font-size");
      pt_text_view_update_css (self);
      return;
    }

  int new_size = self->font_size + amount;

  /* Check for supported range */
  gboolean            in_range;
  GSettingsSchema    *schema;
  GSettingsSchemaKey *key;
  GVariant           *value = g_variant_new ("i", new_size);

  g_object_get (self->settings, "settings-schema", &schema, NULL);
  key = g_settings_schema_get_key (schema, "font-size");
  in_range = g_settings_schema_key_range_check (key, value);

  g_settings_schema_unref (schema);
  g_settings_schema_key_unref (key);
  g_variant_unref (value);

  if (in_range && g_settings_set_int (self->settings, "font-size", new_size))
    {
      self->font_size = new_size;
      pt_text_view_update_css (self);
    }
}

static void
pt_text_view_action_zoom (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  PtTextView *self = PT_TEXT_VIEW (widget);

  if (g_strcmp0 (action_name, "view.zoom-in") == 0)
    pt_text_view_zoom (self, 1);
  else if (g_strcmp0 (action_name, "view.zoom-out") == 0)
    pt_text_view_zoom (self, -1);
  else if (g_strcmp0 (action_name, "view.zoom-default") == 0)
    pt_text_view_zoom (self, 0);
  else
    g_assert_not_reached ();
}

static void
released_cb (GtkGestureClick *gesture,
             guint            n_press,
             double           x,
             double           y,
             PtTextView      *self)
{
  GtkTextIter iter;
  int         tx, ty;

  if (gtk_gesture_single_get_button (GTK_GESTURE_SINGLE (gesture)) > 1)
    return;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (self),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &tx, &ty);

  if (gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self), &iter, tx, ty))
    {
      goto_timestamp_at_iter (self, iter);
    }
}

static gboolean hovering_over_link = FALSE;

static void
set_cursor_if_appropriate (PtTextView *self,
                           int         x,
                           int         y)
{
  GSList     *tags = NULL, *tagp = NULL;
  GtkTextIter iter;
  gboolean    hovering = FALSE;

  if (gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self), &iter, x, y))
    {
      tags = gtk_text_iter_get_tags (&iter);
      for (tagp = tags; tagp != NULL; tagp = tagp->next)
        {
          GtkTextTag *tag = tagp->data;
          if (tag == self->timestamp_tag)
            {
              hovering = TRUE;
              break;
            }
        }
    }

  if (hovering != hovering_over_link)
    {
      hovering_over_link = hovering;

      if (hovering_over_link)
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "default");
      else
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "text");
    }

  if (tags)
    g_slist_free (tags);
}

static void
motion_cb (GtkEventControllerMotion *controller,
           double                    x,
           double                    y,
           PtTextView               *self)
{
  int tx, ty;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (self),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &tx, &ty);

  set_cursor_if_appropriate (self, tx, ty);
}

static gboolean
on_scroll_scrolled_cb (GtkEventControllerScroll *scroll,
                       double                    dx,
                       double                    dy,
                       PtTextView               *self)
{
  GdkModifierType mods;

  mods = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));

  if ((mods & GDK_CONTROL_MASK) != 0)
    {
      pt_text_view_zoom (self, dy < 0 ? 1 : -1);
      return TRUE;
    }

  return FALSE;
}

void
css_parsing_error (GtkCssProvider *self,
                   GtkCssSection  *section,
                   GError         *error,
                   gpointer        user_data)
{
  g_printerr ("CSS error: %s\n", error->message);
}

static void
timestamp_clicked (GtkButton *button,
                   gpointer   user_data)
{
  PtTextView *self = PT_TEXT_VIEW (user_data);
  char       *timestamp = g_strdup (gtk_button_get_label (button));
  pt_player_goto_timestamp (self->player, timestamp);
  g_free (timestamp);
}

void
pt_text_view_insert_timestamp (PtTextView *self)
{
  GtkTextChildAnchor *anchor;
  GtkTextIter         iter;
  char               *timestamp;

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), &iter,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer)));

  /* Don't insert into existing timestamp */
  timestamp = get_timestamp_at_iter (self, iter);
  if (timestamp != NULL)
    {
      g_free (timestamp);
      return;
    }

  timestamp = pt_player_get_timestamp (self->player);
  if (timestamp == NULL)
    return;

  anchor = gtk_text_child_anchor_new ();
  gtk_text_buffer_insert_child_anchor (GTK_TEXT_BUFFER (self->buffer), &iter, anchor);
  GtkWidget *child = gtk_button_new_with_label (timestamp);
  gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (self), child, anchor);
  g_signal_connect (child, "clicked", G_CALLBACK (timestamp_clicked), self);

  g_free (timestamp);
}

static void
on_notify_document_font_name_cb (PtTextView      *self,
                                 GParamSpec      *pspec,
                                 AdwStyleManager *style_manager)
{
  const gchar *font = adw_style_manager_get_document_font_name (style_manager);
  g_clear_pointer (&self->font_desc, pango_font_description_free);
  self->font_desc = pango_font_description_from_string (font);
  pt_text_view_update_css (self);
}

void
pt_text_view_connect (PtTextView *self)
{
  PtWindow *win = PT_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  self->player = _pt_window_get_player (win);
  self->settings = _pt_window_get_settings (win);
  self->font_size = g_settings_get_int (self->settings, "font-size");
  pt_text_view_update_css (self);
}

void
insert_timestamp_button_cb (PtTextBuffer       *buffer,
                            GtkTextChildAnchor *anchor,
                            char               *timestamp,
                            gpointer            user_data)
{
  g_print ("callback!\n");
  PtTextView *self = PT_TEXT_VIEW (user_data);
  GtkWidget  *child = gtk_button_new_with_label (timestamp);
  gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (self), child, anchor);
  g_signal_connect (child, "clicked", G_CALLBACK (timestamp_clicked), self);
}

static void
pt_text_view_copy_clipboard (GtkTextView *text_view)
{
  g_print ("%s\n", __func__);
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));
  PtTextView   *self = PT_TEXT_VIEW (text_view);

  pt_text_buffer_copy_clipboard (self->buffer, clipboard);
}

static void
pt_text_view_cut_clipboard (GtkTextView *text_view)
{
  g_print ("%s\n", __func__);
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));
  PtTextView   *self = PT_TEXT_VIEW (text_view);

  pt_text_buffer_cut_clipboard (self->buffer, clipboard, TRUE);
}

static void
pt_text_view_paste_clipboard (GtkTextView *text_view)
{
  g_print ("%s\n", __func__);
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));
  PtTextView   *self = PT_TEXT_VIEW (text_view);

  /*text_view->priv->scroll_after_paste = TRUE;*/

  pt_text_buffer_paste_clipboard (self->buffer,
                                  clipboard);
}

static void
pt_text_view_dispose (GObject *object)
{
  PtTextView *self = PT_TEXT_VIEW (object);

  g_clear_object (&self->css_provider);
  g_clear_pointer (&self->font_desc, pango_font_description_free);

  G_OBJECT_CLASS (pt_text_view_parent_class)->dispose (object);
}

static void
pt_text_view_init (PtTextView *self)
{
  self->buffer = pt_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self), GTK_TEXT_BUFFER (self->buffer));
  g_signal_connect (self->buffer, "insert-timestamp-button", G_CALLBACK (insert_timestamp_button_cb), self);

  /* Create timestamp tag */
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self->buffer), "timestamp",
                              "family", "monospace",
                              "background", "yellow",
                              "editable", false,
                              NULL);
  GtkTextTagTable *tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self->buffer));
  self->timestamp_tag = gtk_text_tag_table_lookup (tag_table, "timestamp");

  /* experimental */
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self->buffer), "widget",
                              "invisible", TRUE,
                              NULL);

  /* Controllers */
  GtkEventController *controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  /*controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (controller, "released", G_CALLBACK (released_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);*/

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect (controller, "scroll", G_CALLBACK (on_scroll_scrolled_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  /* CSS */
  self->css_provider = gtk_css_provider_new ();
  g_signal_connect (self->css_provider, "parsing-error", G_CALLBACK (css_parsing_error), NULL);

  GdkDisplay *display = gdk_display_get_default ();
  gtk_style_context_add_provider_for_display (display, GTK_STYLE_PROVIDER (self->css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self), GTK_WRAP_WORD);

  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  const gchar     *font = adw_style_manager_get_document_font_name (style_manager);
  self->font_desc = pango_font_description_from_string (font);
  g_signal_connect_object (style_manager,
                           "notify::document-font-name",
                           G_CALLBACK (on_notify_document_font_name_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
pt_text_view_class_init (PtTextViewClass *klass)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (klass);
  GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS (klass);

  object_class->dispose = pt_text_view_dispose;
  text_view_class->copy_clipboard = pt_text_view_copy_clipboard;
  text_view_class->cut_clipboard = pt_text_view_cut_clipboard;
  text_view_class->paste_clipboard = pt_text_view_paste_clipboard;

  gtk_widget_class_install_action (widget_class, "view.zoom-in", NULL, pt_text_view_action_zoom);
  gtk_widget_class_install_action (widget_class, "view.zoom-out", NULL, pt_text_view_action_zoom);
  gtk_widget_class_install_action (widget_class, "view.zoom-default", NULL, pt_text_view_action_zoom);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_plus, GDK_CONTROL_MASK, "view.zoom-in", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Add, GDK_CONTROL_MASK, "view.zoom-in", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_minus, GDK_CONTROL_MASK, "view.zoom-out", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Subtract, GDK_CONTROL_MASK, "view.zoom-out", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_0, GDK_CONTROL_MASK, "view.zoom-default", NULL);
}

PtTextView *
pt_text_view_new (void)
{
  return g_object_new (PT_TYPE_TEXT_VIEW, NULL);
}
