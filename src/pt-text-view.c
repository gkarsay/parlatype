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

#include "pt-file-loader.h"
#include "pt-file-saver.h"
#include "pt-text-buffer.h"
#include "pt-timestamp-button.h"
#include "pt-window-private.h"

struct _PtTextView
{
  GtkTextView parent;

  PtTextBuffer         *buffer;
  GtkCssProvider       *css_provider;
  int                   font_size;
  PangoFontDescription *font_desc;

  PtPlayer  *player;
  GSettings *settings;

  GFile *audio;

  GQueue   timestamps;
  gboolean timestamps_visible;
};

typedef struct
{
  gulong handler_id;
  GFile *file;
} Load;

enum
{
  PROP_0,
  PROP_AUDIO_FILE,
  PROP_TIMESTAMPS_VISIBLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtTextView, pt_text_view, GTK_TYPE_TEXT_VIEW)

static GParamSpec *properties[N_PROPS];

static gboolean
possible_anchor (gunichar ch,
                 gpointer user_data)
{
  return ch == PT_TEXT_UNKNOWN_CHAR;
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
      gtk_text_iter_backward_char (&iter);
      anchor = gtk_text_iter_get_child_anchor (&iter);
      if (anchor != NULL && gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (self)))
        {
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
      anchor = gtk_text_iter_get_child_anchor (&iter);
      if (anchor != NULL && gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (self)))
        {
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
  char *css = g_strdup_printf ("textview        {font-family: %s; "
                               "font-size: %dpt;} "
                               "textview button {font-size: calc(%dpt * 0.6); "
                               "line-height: 0; "
                               "padding: 0; "
                               "margin-top: 0; "
                               "margin-bottom: 0; "
                               "margin-left: 0.5em; "
                               "margin-right: 0.5em;}",
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

gboolean
pt_text_view_get_timestamps_visible (PtTextView *self)
{
  g_return_val_if_fail (PT_IS_TEXT_VIEW (self), FALSE);

  return self->timestamps_visible;
}

void
pt_text_view_set_timestamps_visible (PtTextView *self,
                                     gboolean    visible)
{
  g_return_if_fail (PT_IS_TEXT_VIEW (self));

  if (self->timestamps_visible == visible)
    return;

  self->timestamps_visible = visible;

  for (const GList *list = self->timestamps.head; list; list = list->next)
    {
      GtkWidget *child = list->data;
      gtk_widget_set_visible (child, visible);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMESTAMPS_VISIBLE]);
}

/*static gboolean hovering_over_link = FALSE;

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
}*/

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
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                    "MESSAGE", "CSS error: %s\n", error->message);
}

static void
timestamp_clicked (GtkButton *button,
                   gpointer   user_data)
{
  PtTextView *self = PT_TEXT_VIEW (user_data);
  char       *timestamp = g_strdup (pt_timestamp_button_get_timestamp (PT_TIMESTAMP_BUTTON (button)));
  pt_player_goto_timestamp (self->player, timestamp);
  g_free (timestamp);
}

static void
delete_range_cb (GtkTextBuffer     *buffer,
                 const GtkTextIter *start,
                 const GtkTextIter *end,
                 gpointer           user_data)
{
  /* If user deletes something, check if there are Timestamps in the range
   * and remove them from GQUeue self->timestamps. */

  PtTextView         *self = PT_TEXT_VIEW (user_data);
  GtkTextIter         search = *start;
  GtkTextChildAnchor *anchor;
  GtkWidget         **widgets;
  guint               out_len;
  gboolean            check_me;

  check_me = gtk_text_iter_get_char (&search) == PT_TEXT_UNKNOWN_CHAR;

  do
    {
      if (check_me)
        {
          anchor = gtk_text_iter_get_child_anchor (&search);
          if (anchor != NULL)
            {
              widgets = gtk_text_child_anchor_get_widgets (anchor, &out_len);
              g_assert (out_len == 1);
              GList *link = g_queue_find (&self->timestamps, widgets[0]);
              g_assert (link != NULL);
              g_queue_delete_link (&self->timestamps, link);
              g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                                "MESSAGE", "timestamp removed, total: %d", g_queue_get_length (&self->timestamps));
            }
        }
      check_me = TRUE;
    }
  while (gtk_text_iter_forward_find_char (&search, possible_anchor, NULL, end));
}

static void
insert_timestamp_button_cb (GObject            *object,
                            GtkTextChildAnchor *anchor,
                            char               *timestamp,
                            gpointer            user_data)
{
  PtTextView *self = PT_TEXT_VIEW (user_data);
  GtkWidget  *child = pt_timestamp_button_new (timestamp);
  gtk_widget_set_visible (child, self->timestamps_visible);
  gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (self), child, anchor);
  g_signal_connect (child, "clicked", G_CALLBACK (timestamp_clicked), self);
  g_queue_push_head (&self->timestamps, child);
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "timestamp added, total: %d", g_queue_get_length (&self->timestamps));
}

void
pt_text_view_insert_timestamp (PtTextView *self)
{
  GtkTextChildAnchor *anchor;
  GtkTextIter         iter;
  char               *timestamp;

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), &iter,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer)));

  timestamp = pt_player_get_timestamp (self->player);
  if (timestamp == NULL)
    return;

  /* Create a new anchor, never add more than one widget to an anchor */
  anchor = gtk_text_child_anchor_new ();
  gtk_text_buffer_insert_child_anchor (GTK_TEXT_BUFFER (self->buffer), &iter, anchor);

  insert_timestamp_button_cb (NULL, anchor, timestamp, self);

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

static void
pt_text_view_copy_clipboard (GtkTextView *text_view)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));
  PtTextView   *self = PT_TEXT_VIEW (text_view);

  pt_text_buffer_copy_clipboard (self->buffer, clipboard);
}

static void
pt_text_view_cut_clipboard (GtkTextView *text_view)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));
  PtTextView   *self = PT_TEXT_VIEW (text_view);

  pt_text_buffer_cut_clipboard (self->buffer, clipboard, TRUE);
}

static void
pt_text_view_paste_clipboard (GtkTextView *text_view)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));
  PtTextView   *self = PT_TEXT_VIEW (text_view);

  /*text_view->priv->scroll_after_paste = TRUE;*/

  pt_text_buffer_paste_clipboard (self->buffer,
                                  clipboard);
}

static void
pt_text_view_save_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PtFileSaver *saver = PT_FILE_SAVER (source_object);
  GTask       *task = user_data;
  GError      *error = NULL;

  if (!pt_file_saver_save_finish (saver, res, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_boolean (task, TRUE);
}

gboolean
pt_text_view_save_finish (PtTextView   *self,
                          GAsyncResult *result,
                          GError      **error)
{
  g_return_val_if_fail (PT_IS_TEXT_VIEW (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
pt_text_view_save_async (PtTextView         *self,
                         GFile              *file,
                         GCancellable       *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer            user_data)
{
  PtFileSaver *saver;
  GTask       *task;

  g_return_if_fail (PT_IS_TEXT_VIEW (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  task = g_task_new (self, cancellable, callback, user_data);

  saver = pt_file_saver_new (self, file);
  pt_file_saver_save_async (saver,
                            G_PRIORITY_DEFAULT,
                            NULL,
                            pt_text_view_save_cb,
                            g_steal_pointer (&task));
}

gboolean
pt_text_view_load (PtTextView *self,
                   GFile      *file,
                   GError    **error)
{
  PtFileLoader *loader;
  gulong        handler_id;
  gboolean      success;

  g_return_val_if_fail (PT_IS_TEXT_VIEW (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  loader = pt_file_loader_new (self, file);
  handler_id = g_signal_connect (loader, "insert-timestamp-button", G_CALLBACK (insert_timestamp_button_cb), self);

  success = pt_file_loader_load (loader, error);

  g_signal_handler_disconnect (loader, handler_id);
  g_object_unref (loader);

  return success;
}

const GFile *
pt_text_view_get_audio_file (PtTextView *self)
{
  g_return_val_if_fail (PT_IS_TEXT_VIEW (self), NULL);

  return self->audio;
}

void
pt_text_view_set_audio_file (PtTextView *self,
                             GFile      *file)
{
  g_return_if_fail (PT_IS_TEXT_VIEW (self));
  g_return_if_fail (G_IS_FILE (file) || file == NULL);

  if (file == NULL && self->audio == NULL)
    return;

  if (file != NULL &&
      self->audio != NULL &&
      g_file_equal (file, self->audio))
    return;

  g_clear_object (&self->audio);

  if (file != NULL)
    self->audio = g_object_ref (file);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUDIO_FILE]);
}

static void
pt_text_view_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  PtTextView *self = PT_TEXT_VIEW (object);

  switch (prop_id)
    {
    case PROP_AUDIO_FILE:
      pt_text_view_set_audio_file (self, g_value_get_object (value));
      break;

    case PROP_TIMESTAMPS_VISIBLE:
      pt_text_view_set_timestamps_visible (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_text_view_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  PtTextView *self = PT_TEXT_VIEW (object);

  switch (prop_id)
    {
    case PROP_AUDIO_FILE:
      g_value_set_object (value, self->audio);
      break;

    case PROP_TIMESTAMPS_VISIBLE:
      g_value_set_boolean (value, self->timestamps_visible);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_text_view_dispose (GObject *object)
{
  PtTextView *self = PT_TEXT_VIEW (object);

  g_clear_object (&self->css_provider);
  g_clear_pointer (&self->font_desc, pango_font_description_free);
  g_queue_clear (&self->timestamps);
  g_clear_object (&self->audio);

  G_OBJECT_CLASS (pt_text_view_parent_class)->dispose (object);
}

static void
pt_text_view_init (PtTextView *self)
{
  self->buffer = pt_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self), GTK_TEXT_BUFFER (self->buffer));
  g_signal_connect (self->buffer, "insert-timestamp-button", G_CALLBACK (insert_timestamp_button_cb), self);
  g_signal_connect (self->buffer, "delete-range", G_CALLBACK (delete_range_cb), self);

  /* Controllers */
  GtkEventController *controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  /*controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);*/

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

  object_class->get_property = pt_text_view_get_property;
  object_class->set_property = pt_text_view_set_property;
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

  properties[PROP_AUDIO_FILE] =
      g_param_spec_object ("audio", NULL, NULL,
                           G_TYPE_FILE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_TIMESTAMPS_VISIBLE] =
      g_param_spec_boolean ("timestamps-visible", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

PtTextView *
pt_text_view_new (void)
{
  return g_object_new (PT_TYPE_TEXT_VIEW, NULL);
}
