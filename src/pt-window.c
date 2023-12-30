/* Copyright (C) Gabor Karsay 2016-2019 <gabor.karsay@gmx.at>
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

#include "pt-window.h"

#include "pt-app.h"
#include "pt-goto-dialog.h"
#include "pt-window-dnd.h"
#include "pt-window-private.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <parlatype.h>
#include <stdlib.h> /* exit() */

struct _PtWindow
{
  GtkApplicationWindow parent;
  PtPlayer            *player;
  GtkWidget           *waveviewer;
  GSettings           *editor;
  GtkWidget           *primary_menu_button;
  GtkWidget           *pos_menu_button;

  GtkRecentManager *recent;
  PtConfig         *asr_config;

  GdkClipboard *clip;
  gulong        clip_handler_id;

  /* Headerbar widgets */
  GtkWidget *button_open;

  /* Main window widgets */
  GtkWidget  *controls_row_box;
  GtkWidget  *controls_box;
  GtkWidget  *progress;
  GtkWidget  *button_play;
  GtkWidget  *button_jump_back;
  GtkWidget  *button_jump_forward;
  GtkWidget  *volumebutton;
  GStrv       vol_icons;
  GtkGesture *vol_event;
  GMenuItem  *go_to_timestamp;
  GtkWidget  *speed_scale;

  GMenuModel *primary_menu;
  GMenuModel *secondary_menu;
  GMenu      *asr_menu;
  GMenuItem  *asr_menu_item1;
  GMenuItem  *asr_menu_item2;
  gboolean    asr;

  gint64 last_time; // last time to compare if it changed

  gint    timer;
  gdouble speed;
};

G_DEFINE_TYPE (PtWindow, pt_window, GTK_TYPE_APPLICATION_WINDOW)

static void play_button_toggled_cb (GtkToggleButton *button, PtWindow *self);

void
pt_error_message (PtWindow    *parent,
                  const gchar *message,
                  const gchar *secondary_message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (
      GTK_WINDOW (parent),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_OK,
      "%s", message);

  if (secondary_message)
    gtk_message_dialog_format_secondary_text (
        GTK_MESSAGE_DIALOG (dialog),
        "%s", secondary_message);

  g_signal_connect_swapped (dialog, "response",
                            G_CALLBACK (gtk_window_destroy), dialog);

  gtk_window_present (GTK_WINDOW (dialog));
}

void
copy_timestamp (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  PtWindow    *self = PT_WINDOW (user_data);
  const gchar *timestamp = NULL;

  timestamp = pt_player_get_timestamp (self->player);
  if (timestamp)
    gdk_clipboard_set_text (self->clip, timestamp);
}

static void
clip_text_cb (GdkClipboard *clip,
              GAsyncResult *res,
              gpointer      data)
{
  PtWindow *self = (PtWindow *) data;
  GError   *error = NULL;
  gchar    *timestamp;

  timestamp = gdk_clipboard_read_text_finish (clip, res, &error);

  if (!timestamp)
    {
      pt_error_message (self, _ ("Error"), error->message);
      g_clear_error (&error);
      return;
    }

  pt_player_goto_timestamp (self->player, timestamp);
  pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
  g_free (timestamp);
}

void
insert_timestamp (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  gdk_clipboard_read_text_async (
      self->clip,
      NULL,
      (GAsyncReadyCallback) clip_text_cb,
      self);
}

static void
goto_dialog_response_cb (GtkDialog *dlg,
                         gint       response_id,
                         gpointer   user_data)
{
  PtWindow *self = PT_WINDOW (user_data);
  gint      pos;

  if (response_id == GTK_RESPONSE_OK)
    {
      pos = pt_goto_dialog_get_pos (PT_GOTO_DIALOG (dlg));
      pt_player_jump_to_position (self->player, pos * 1000);
      pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
    }

  gtk_window_destroy (GTK_WINDOW (dlg));
}

void
goto_position (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  PtGotoDialog *dlg;

  dlg = pt_goto_dialog_new (GTK_WINDOW (self));
  pt_goto_dialog_set_pos (dlg, pt_player_get_position (self->player) / 1000);
  pt_goto_dialog_set_max (dlg, pt_player_get_duration (self->player) / 1000);

  g_signal_connect (dlg, "response",
                    G_CALLBACK (goto_dialog_response_cb), self);

  gtk_window_present (GTK_WINDOW (dlg));
}

void
goto_cursor (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
}

void
jump_back (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  pt_player_jump_back (self->player);
}

void
jump_forward (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  pt_player_jump_forward (self->player);
}

void
play (GSimpleAction *action,
      GVariant      *parameter,
      gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  pt_player_play_pause (self->player);
}

static void
set_zoom (GSettings *editor,
          gint       step)
{
  gint pps;

  pps = g_settings_get_int (editor, "pps");
  if ((step > 0 && pps >= 200) || (step < 0 && pps <= 25))
    return;

  g_settings_set_int (editor, "pps", CLAMP (pps + step, 25, 200));
}

void
zoom_in (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);
  set_zoom (self->editor, 25);
}

void
zoom_out (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);
  set_zoom (self->editor, -25);
}

static gboolean
setup_asr (PtWindow *self)
{
  GError  *error = NULL;
  gboolean success;

  success = pt_player_configure_asr (
      self->player,
      self->asr_config,
      &error);

  if (success)
    pt_player_set_mode (self->player, PT_MODE_ASR);
  else
    pt_error_message (self, error->message, NULL);

  return success;
}

static void
set_mode_playback (PtWindow *self)
{
  pt_player_set_mode (self->player, PT_MODE_PLAYBACK);
}

void
change_mode (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
  PtWindow    *self = PT_WINDOW (user_data);
  const gchar *mode;
  gboolean     success = TRUE;

  /* Work around synchronisation issues in PtPlayer in playing state */
  pt_player_pause (self->player);

  mode = g_variant_get_string (state, NULL);
  if (g_strcmp0 (mode, "playback") == 0)
    {
      set_mode_playback (self);
    }
  else if (g_strcmp0 (mode, "asr") == 0)
    {
      if (self->asr_config != NULL)
        {
          success = setup_asr (self);
        }
      else
        {
          return;
        }
    }
  else
    {
      g_assert_not_reached ();
    }

  if (success)
    g_simple_action_set_state (action, state);
}

const GActionEntry win_actions[] = {
  { "mode", NULL, "s", "'playback'", change_mode },
  { "copy", copy_timestamp, NULL, NULL, NULL },
  { "insert", insert_timestamp, NULL, NULL, NULL },
  { "goto", goto_position, NULL, NULL, NULL },
  { "goto-cursor", goto_cursor, NULL, NULL, NULL },
  { "zoom-in", zoom_in, NULL, NULL, NULL },
  { "zoom-out", zoom_out, NULL, NULL, NULL },
  { "jump-back", jump_back, NULL, NULL, NULL },
  { "jump-forward", jump_forward, NULL, NULL, NULL },
  { "play", play, NULL, NULL, NULL }
};

static void
update_time (PtWindow *self)
{
  gchar *text;
  gint64 time;

  time = pt_player_get_position (self->player);

  if (time == -1)
    return;

  /* Update label only if time has changed at 10th seconds level */
  if (time / 100 != self->last_time)
    {
      text = pt_player_get_current_time_string (
          self->player,
          PT_PRECISION_SECOND_10TH);

      if (text == NULL)
        return;

      gtk_menu_button_set_label (GTK_MENU_BUTTON (self->pos_menu_button), text);
      g_free (text);
      self->last_time = time / 100;
    }

  g_object_set (self->waveviewer, "playback-cursor", time, NULL);
}

static gboolean
update_time_tick (GtkWidget     *widget,
                  GdkFrameClock *frame_clock,
                  gpointer       data)
{
  PtWindow *self = (PtWindow *) data;
  update_time (self);
  return G_SOURCE_CONTINUE;
}

static void
add_timer (PtWindow *self)
{
  if (self->timer == 0)
    {
      self->timer = gtk_widget_add_tick_callback (
          self->waveviewer,
          update_time_tick,
          self,
          NULL);
    }
}

static void
remove_timer (PtWindow *self)
{
  if (self->timer > 0)
    {
      gtk_widget_remove_tick_callback (self->waveviewer,
                                       self->timer);
      self->timer = 0;
    }
}

static void
change_jump_back_tooltip (PtWindow *self)
{
  gchar *back;
  gint   seconds;

  seconds = pt_player_get_back (self->player) / 1000;
  back = g_strdup_printf (
      ngettext ("Skip back 1 second",
                "Skip back %d seconds",
                seconds),
      seconds);

  gtk_widget_set_tooltip_text (self->button_jump_back, back);
  g_free (back);
}

static void
change_jump_forward_tooltip (PtWindow *self)
{
  gchar *forward;
  gint   seconds;

  seconds = pt_player_get_forward (self->player) / 1000;
  forward = g_strdup_printf (
      ngettext ("Skip forward 1 second",
                "Skip forward %d seconds",
                seconds),
      seconds);

  gtk_widget_set_tooltip_text (self->button_jump_forward, forward);
  g_free (forward);
}

static void
change_play_button_tooltip (PtWindow *self)
{
  gchar   *play;
  gint     pause;
  gboolean free_me = FALSE;

  pause = pt_player_get_pause (self->player) / 1000;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button_play)))
    {
      if (pause == 0)
        {
          play = _ ("Pause");
        }
      else
        {
          play = g_strdup_printf (
              ngettext ("Pause and rewind 1 second",
                        "Pause and rewind %d seconds",
                        pause),
              pause);
          free_me = TRUE;
        }
    }
  else
    {
      play = _ ("Start playing");
    }

  gtk_widget_set_tooltip_text (self->button_play, play);
  if (free_me)
    g_free (play);
}

static void
update_insert_action_sensitivity_cb (GdkClipboard *clip,
                                     GAsyncResult *res,
                                     gpointer      data)
{
  PtWindow *self = PT_WINDOW (data);
  PtPlayer *player = self->player;
  gchar    *text;
  gchar    *timestamp = NULL;
  gboolean  result = FALSE;
  GAction  *action;
  gint      pos;
  gchar    *label;

  text = gdk_clipboard_read_text_finish (clip, res, NULL);

  if (text)
    {
      pos = pt_player_get_timestamp_position (player, text, TRUE);
      timestamp = NULL;
      if (pos >= 0)
        {
          result = TRUE;
          timestamp = pt_player_get_time_string (
              pos,
              pt_player_get_duration (player),
              PT_PRECISION_SECOND_10TH);
        }
    }

  if (timestamp)
    label = g_strdup_printf (_ ("Go to Time in Clipboard: %s"), timestamp);
  else
    label = g_strdup (_ ("Go to Time in Clipboard"));

  g_menu_item_set_label (self->go_to_timestamp, label);
  g_menu_remove (G_MENU (self->secondary_menu), 1);
  g_menu_insert_item (G_MENU (self->secondary_menu), 1, self->go_to_timestamp);
  g_free (label);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "insert");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), result);
}

static void
update_insert_action_sensitivity (GdkClipboard *clip,
                                  gpointer      data)
{
  PtWindow *self = PT_WINDOW (data);

  gdk_clipboard_read_text_async (
      clip,
      NULL,
      (GAsyncReadyCallback) update_insert_action_sensitivity_cb,
      self);
}

static void
update_goto_cursor_action_sensitivity (GObject    *gobject,
                                       GParamSpec *pspec,
                                       gpointer    user_data)
{
  PtWaveviewer *viewer = PT_WAVEVIEWER (gobject);
  PtWindow     *self = PT_WINDOW (user_data);
  GAction      *action;
  gboolean      follow;

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "goto-cursor");
  follow = pt_waveviewer_get_follow_cursor (viewer);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !follow);
}

static void
enable_win_actions (PtWindow *self,
                    gboolean  state)
{
  GAction *action;

  /* always active */
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "mode");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "copy");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);

  /* always disable on request, enable only conditionally */
  if (!state)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (self), "insert");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);
    }
  else
    {
      update_insert_action_sensitivity (self->clip, self);
    }

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "goto");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);

  /* always insensitive: either there is no waveform or we are already at cursor position */
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "goto-cursor");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "zoom-in");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "zoom-out");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);
}

static void
pt_window_ready_to_play (PtWindow *self,
                         gboolean  state)
{
  /* Set up widget sensitivity/visibility, actions, labels, window title
     and timer according to the state of PtPlayer (ready to play or not).
     Reset tooltips for insensitive widgets. */

  gchar *display_name = NULL;

  enable_win_actions (self, state);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button_play), FALSE);

  gtk_widget_set_sensitive (self->button_play, state);
  gtk_widget_set_sensitive (self->button_jump_back, state);
  gtk_widget_set_sensitive (self->button_jump_forward, state);
  gtk_widget_set_sensitive (self->speed_scale, state);
  gtk_widget_set_sensitive (self->volumebutton, state);

  if (state)
    {
      gtk_widget_remove_css_class (self->button_open, "suggested-action");
      display_name = pt_player_get_filename (self->player);
      if (display_name)
        {
          gtk_window_set_title (GTK_WINDOW (self), display_name);
          g_free (display_name);
        }

      gchar *uri = NULL;
      uri = pt_player_get_uri (self->player);
      if (uri)
        {
          gtk_recent_manager_add_item (self->recent, uri);
          g_free (uri);
        }

      change_play_button_tooltip (self);
      change_jump_back_tooltip (self);
      change_jump_forward_tooltip (self);
      pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
      add_timer (self);
    }
  else
    {
      gtk_menu_button_set_label (GTK_MENU_BUTTON (self->pos_menu_button), "00:00.0");
      gtk_window_set_title (GTK_WINDOW (self), "Parlatype");
      gtk_widget_set_tooltip_text (self->button_jump_back, NULL);
      gtk_widget_set_tooltip_text (self->button_jump_forward, NULL);
      remove_timer (self);
    }
}

static void
player_error_cb (PtPlayer *player,
                 GError   *error,
                 PtWindow *self)
{
  pt_window_ready_to_play (self, FALSE);
  pt_error_message (self, error->message, NULL);
}

static void
open_cb (PtWaveviewer *viewer,
         GAsyncResult *res,
         gpointer     *data)
{
  PtWindow *self = (PtWindow *) data;
  GError   *error = NULL;

  if (!pt_waveviewer_load_wave_finish (viewer, res, &error))
    {
      pt_error_message (self, error->message, NULL);
      g_error_free (error);
      /* Very unlikely situation: Stream is open and playable,
       * but loading the waveform failed. */
    }
}

gchar *
pt_window_get_uri (PtWindow *self)
{
  return pt_player_get_uri (self->player);
}

void
pt_window_open_file (PtWindow *self,
                     gchar    *uri)
{
  g_return_if_fail (uri != NULL);

  gchar *current_uri;
  gint   cmp = 1;

  /* Don't reload already loaded waveform */
  current_uri = pt_player_get_uri (self->player);
  if (current_uri)
    {
      cmp = g_strcmp0 (current_uri, uri);
      g_free (current_uri);
    }
  if (cmp == 0)
    return;

  pt_window_ready_to_play (self, FALSE);
  if (!pt_player_open_uri (self->player, uri))
    return;
  pt_window_ready_to_play (self, TRUE);
  pt_waveviewer_load_wave_async (PT_WAVEVIEWER (self->waveviewer),
                                 uri,
                                 NULL,
                                 (GAsyncReadyCallback) open_cb,
                                 self);
}

static void
update_play_after_toggle (PtWindow        *self,
                          GtkToggleButton *button)
{
  if (gtk_toggle_button_get_active (button))
    {
      update_time (self);
      pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
    }

  change_play_button_tooltip (self);
}

static void
player_play_toggled_cb (PtPlayer *player,
                        PtWindow *self)
{
  GtkToggleButton *play;
  play = GTK_TOGGLE_BUTTON (self->button_play);

  /* Player changed play/pause; toggle GUI button and block signals */
  g_signal_handlers_block_by_func (play, play_button_toggled_cb, self);
  gtk_toggle_button_set_active (play, !gtk_toggle_button_get_active (play));
  g_signal_handlers_unblock_by_func (play, play_button_toggled_cb, self);

  update_play_after_toggle (self, play);
}

static void
play_button_toggled_cb (GtkToggleButton *button,
                        PtWindow        *self)
{
  /* GUI button toggled, block signals from PtPlayer */
  g_signal_handlers_block_by_func (self->player, player_play_toggled_cb, self);
  pt_player_play_pause (self->player);
  g_signal_handlers_unblock_by_func (self->player, player_play_toggled_cb, self);

  update_play_after_toggle (self, button);
}

static void
player_end_of_stream_cb (PtPlayer *player,
                         PtWindow *self)
{
  /* Pause player without jumping back, this will also toggle the
   * Play button */
  pt_player_pause (self->player);
  change_play_button_tooltip (self);
}

static void
pt_window_direction_changed (GtkWidget       *widget,
                             GtkTextDirection previous_direction)
{
  /* In RTL layouts playback control elements are *not* supposed to be
   * mirrored. Undo automatic mirroring for those elements. */

  PtWindow *self = PT_WINDOW (widget);
  GtkScale *speed_scale = GTK_SCALE (self->speed_scale);

  gtk_widget_set_direction (self->button_jump_back, GTK_TEXT_DIR_LTR);
  gtk_widget_set_direction (self->button_jump_forward, GTK_TEXT_DIR_LTR);
  gtk_widget_set_direction (self->controls_box, GTK_TEXT_DIR_LTR);

  if (previous_direction == GTK_TEXT_DIR_LTR)
    gtk_scale_set_value_pos (speed_scale, GTK_POS_LEFT);
  else
    gtk_scale_set_value_pos (speed_scale, GTK_POS_RIGHT);
}

static void
set_asr_config (PtWindow *self)
{
  GFile       *asr_file;
  gchar       *asr_path;
  GAction     *action;
  GVariant    *variant;
  const gchar *mode;

  /* called when config file changed (and on startup);
   * get rid of old config object first */
  if (self->asr_config)
    g_clear_object (&self->asr_config);

  /* get new config object */
  asr_path = g_settings_get_string (self->editor, "asr-config");
  asr_file = g_file_new_for_path (asr_path);
  self->asr_config = pt_config_new (asr_file);
  g_object_unref (asr_file);
  g_free (asr_path);

  /* get current mode */
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "mode");
  variant = g_action_get_state (action);
  mode = g_variant_get_string (variant, NULL);

  /* not valid: clear object, remove menu, change to playback mode */
  if (!pt_config_is_valid (self->asr_config) ||
      !pt_player_config_is_loadable (self->player, self->asr_config) ||
      !pt_config_is_installed (self->asr_config))
    {
      g_clear_object (&self->asr_config);
      if (self->asr)
        {
          g_menu_remove (G_MENU (self->primary_menu), 0);
          self->asr = FALSE;
        }
      if (g_strcmp0 (mode, "asr") == 0)
        {
          set_mode_playback (self);
        }
      g_variant_unref (variant);
      return;
    }

  /* valid: add menu, setup ASR plugin */
  if (!self->asr)
    {
      g_menu_prepend_section (G_MENU (self->primary_menu),
                              NULL, /* label */
                              G_MENU_MODEL (self->asr_menu));
      self->asr = TRUE;
    }

  if (g_strcmp0 (mode, "asr") == 0)
    setup_asr (self);

  g_variant_unref (variant);
}

static void
settings_changed_cb (GSettings *settings,
                     gchar     *key,
                     PtWindow  *self)
{
  if (g_strcmp0 (key, "rewind-on-pause") == 0)
    {
      change_play_button_tooltip (self);
      return;
    }

  if (g_strcmp0 (key, "jump-back") == 0)
    {
      change_jump_back_tooltip (self);
      return;
    }

  if (g_strcmp0 (key, "jump-forward") == 0)
    {
      change_jump_forward_tooltip (self);
      return;
    }

  if (g_strcmp0 (key, "asr-config") == 0)
    {
      set_asr_config (self);
      return;
    }
}

static gboolean
map_seconds_to_milliseconds (GValue   *value,
                             GVariant *variant,
                             gpointer  data)
{
  /* Settings store seconds, PtPlayer wants milliseconds */
  gint new;
  new = g_variant_get_int32 (variant);
  new = new * 1000;
  g_value_set_int (value, new);
  return TRUE;
}

static GVariant *
map_milliseconds_to_seconds (const GValue       *value,
                             const GVariantType *type,
                             gpointer            data)
{
  gint new;
  new = g_value_get_int (value);
  new = new / 1000;
  return g_variant_new_int32 (new);
}

static void
setup_settings (PtWindow *self)
{
  self->editor = g_settings_new (APP_ID);

  g_settings_bind (
      self->editor, "pps",
      self->waveviewer, "pps",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (
      self->editor, "rewind-on-pause",
      self->player, "pause",
      G_SETTINGS_BIND_GET,
      map_seconds_to_milliseconds,
      map_milliseconds_to_seconds,
      NULL, NULL);

  g_settings_bind_with_mapping (
      self->editor, "jump-back",
      self->player, "back",
      G_SETTINGS_BIND_GET,
      map_seconds_to_milliseconds,
      map_milliseconds_to_seconds,
      NULL, NULL);

  g_settings_bind_with_mapping (
      self->editor, "jump-forward",
      self->player, "forward",
      G_SETTINGS_BIND_GET,
      map_seconds_to_milliseconds,
      map_milliseconds_to_seconds,
      NULL, NULL);

  g_settings_bind (
      self->editor, "repeat-all",
      self->player, "repeat-all",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "repeat-selection",
      self->player, "repeat-selection",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "show-ruler",
      self->waveviewer, "show-ruler",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "fixed-cursor",
      self->waveviewer, "fixed-cursor",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "timestamp-precision",
      self->player, "timestamp-precision",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "timestamp-fixed",
      self->player, "timestamp-fixed",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "timestamp-delimiter",
      self->player, "timestamp-delimiter",
      G_SETTINGS_BIND_GET);

  g_settings_bind (
      self->editor, "timestamp-fraction-sep",
      self->player, "timestamp-fraction-sep",
      G_SETTINGS_BIND_GET);

  self->asr = FALSE;
  set_asr_config (self);

  /* connect to tooltip changer */

  g_signal_connect (
      self->editor, "changed",
      G_CALLBACK (settings_changed_cb),
      self);

  /* Default speed
     Other solutions would be
     - Automatically save last known speed in GSettings
     - Add a default speed option to preferences dialog
     - Save last known speed in metadata for each file */
  self->speed = 1.0;

  gtk_window_set_default_size (GTK_WINDOW (self),
                               g_settings_get_int (self->editor, "width"),
                               g_settings_get_int (self->editor, "height"));
}

static void
volume_button_update_mute (GObject    *gobject,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
  /* Switch icons of volume button depending on mute state.
   * If muted, only the mute icon is shown (volume can still be adjusted).
   * In normal state restore original icon set. */

  PtWindow       *self = PT_WINDOW (user_data);
  GtkScaleButton *volumebutton = GTK_SCALE_BUTTON (self->volumebutton);
  const gchar    *mute_icon[] = { (gchar *) self->vol_icons[0], NULL };

  if (pt_player_get_mute (self->player))
    {
      gtk_scale_button_set_icons (volumebutton, mute_icon);
    }
  else
    {
      gtk_scale_button_set_icons (volumebutton, (const gchar **) self->vol_icons);
    }
}

static void
volume_button_update_volume (GObject    *gobject,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
  PtWindow       *self = PT_WINDOW (user_data);
  GtkScaleButton *volumebutton = GTK_SCALE_BUTTON (self->volumebutton);
  gdouble         volume = pt_player_get_volume (self->player);

  gtk_scale_button_set_value (volumebutton, volume);
}

static gboolean
volume_button_value_changed_cb (GtkWidget *volumebutton,
                                gdouble    value,
                                gpointer   user_data)
{
  PtWindow *self = PT_WINDOW (user_data);
  pt_player_set_volume (self->player, value);
  return FALSE;
}

static gboolean
volume_button_event_cb (GtkEventControllerLegacy *ctrl,
                        GdkEvent                 *event,
                        gpointer                  user_data)
{
  /* This is for pulseaudiosink in paused state. It doesn't notify of
   * volume/mute changes. Update values when user is interacting with
   * the volume button. */

  PtWindow  *self = PT_WINDOW (user_data);
  GtkWidget *volumebutton;

  volumebutton = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (ctrl));
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (volumebutton),
                              pt_player_get_volume (self->player));
  volume_button_update_mute (NULL, NULL, self);
  return FALSE;
}

static gboolean
volume_button_press_event (GtkGestureClick *gesture,
                           gint             n_press,
                           gdouble          x,
                           gdouble          y,
                           gpointer         user_data)
{
  /* Switch mute state on click with secondary button */

  PtWindow *self = PT_WINDOW (user_data);
  guint     button;
  gboolean  current_mute;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (n_press == 1 && button == GDK_BUTTON_SECONDARY)
    {
      current_mute = pt_player_get_mute (self->player);
      pt_player_set_mute (self->player, !current_mute);
      return TRUE;
    }
  return FALSE;
}

static void
setup_player (PtWindow *self)
{
  self->player = pt_player_new ();
  pt_player_set_mode (self->player, PT_MODE_PLAYBACK);

  pt_player_connect_waveviewer (
      self->player,
      PT_WAVEVIEWER (self->waveviewer));

  g_signal_connect (self->player,
                    "end-of-stream",
                    G_CALLBACK (player_end_of_stream_cb),
                    self);

  g_signal_connect (self->player,
                    "error",
                    G_CALLBACK (player_error_cb),
                    self);

  g_signal_connect (self->player,
                    "play-toggled",
                    G_CALLBACK (player_play_toggled_cb),
                    self);

  GtkAdjustment *speed_adjustment;
  speed_adjustment = gtk_range_get_adjustment (GTK_RANGE (self->speed_scale));
  g_object_bind_property (
      speed_adjustment, "value",
      self->player, "speed",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
setup_volume (PtWindow *self)
{
  /* Connect to volumebutton's "value-changed" signal and to PtPlayer's
   * "notify:volume" signal instead of binding the 2 properties. On
   * startup there is no external Pulseaudio volume yet and the binding
   * would impose our default volume on Pulseaudio. Instead we want to
   * pick up Pulseaudio's volume. That is Parlatype/application specific
   * and it even remembers the last value. The first external volume
   * signal is sent after a file was opened successfully, until then our
   * volume button is insensitive. */
  g_signal_connect (self->volumebutton,
                    "value-changed",
                    G_CALLBACK (volume_button_value_changed_cb),
                    self);

  GtkEventController *v_event;
  v_event = gtk_event_controller_legacy_new ();
  g_signal_connect (v_event,
                    "event",
                    G_CALLBACK (volume_button_event_cb),
                    self);
  gtk_widget_add_controller (self->volumebutton,
                             GTK_EVENT_CONTROLLER (v_event));

  /* Get complete icon set of volume button to change it depending on
   * mute state. */
  g_object_get (self->volumebutton,
                "icons", &self->vol_icons, NULL);

  g_signal_connect (self->player,
                    "notify::mute",
                    G_CALLBACK (volume_button_update_mute),
                    self);

  g_signal_connect (self->player,
                    "notify::volume",
                    G_CALLBACK (volume_button_update_volume),
                    self);

  /* Switch mute state on mouse click with secondary button */
  GtkGesture *vol_event;
  vol_event = gtk_gesture_click_new ();
  gtk_gesture_single_set_exclusive (
      GTK_GESTURE_SINGLE (vol_event), TRUE);
  gtk_gesture_single_set_button (
      GTK_GESTURE_SINGLE (vol_event), 0);
  gtk_event_controller_set_propagation_phase (
      GTK_EVENT_CONTROLLER (vol_event), GTK_PHASE_CAPTURE);
  g_signal_connect (vol_event,
                    "pressed",
                    G_CALLBACK (volume_button_press_event),
                    self);
  gtk_widget_add_controller (self->volumebutton,
                             GTK_EVENT_CONTROLLER (vol_event));
}

static void
setup_accels_actions_menus (PtWindow *self)
{
  /* Actions */
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   win_actions,
                                   G_N_ELEMENTS (win_actions),
                                   self);

  enable_win_actions (self, FALSE);

  /* Setup menus */
  GtkBuilder *builder;

  builder = gtk_builder_new_from_resource ("/org/parlatype/Parlatype/menus.ui");
  self->primary_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "primary-menu"));
  self->secondary_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "secondary-menu"));
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->primary_menu_button), self->primary_menu);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->pos_menu_button), self->secondary_menu);
  g_object_unref (builder);

  /* Setup ASR menu manually; no success with GtkBuilder */
  self->asr_menu = g_menu_new ();
  self->asr_menu_item1 = g_menu_item_new (_ ("Manual Transcription"), "win.mode::playback");
  self->asr_menu_item2 = g_menu_item_new (_ ("Automatic Transcription"), "win.mode::asr");
  g_menu_append_item (self->asr_menu, self->asr_menu_item1);
  g_menu_append_item (self->asr_menu, self->asr_menu_item2);

  self->go_to_timestamp = g_menu_item_new (_ ("Go to Time in Clipboard"), "win.insert");
  g_menu_insert_item (G_MENU (self->secondary_menu), 1, self->go_to_timestamp);
}

static void
progressbar_cb (PtWaveviewer *viewer,
                double        fraction,
                gpointer      user_data)
{
  GtkProgressBar *progressbar = GTK_PROGRESS_BAR (user_data);

  if (fraction == 1)
    fraction = 0;
  gtk_progress_bar_set_fraction (progressbar, fraction);
}

PtPlayer *
_pt_window_get_player (PtWindow *self)
{
  return self->player;
}

GtkRecentManager *
_pt_window_get_recent_manager (PtWindow *self)
{
  return self->recent;
}

GtkWidget *
_pt_window_get_waveviewer (PtWindow *self)
{
  return self->waveviewer;
}

GtkWidget *
_pt_window_get_primary_menu_button (PtWindow *self)
{
  return self->primary_menu_button;
}

GSettings *
_pt_window_get_settings (PtWindow *self)
{
  return self->editor;
}

static void
pt_window_init (PtWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  setup_player (self);
  setup_accels_actions_menus (self);
  setup_settings (self);
  setup_volume (self);
  pt_window_setup_dnd (self); /* this is in pt_window_dnd.c */

  self->recent = gtk_recent_manager_get_default ();
  self->timer = 0;
  self->last_time = 0;
  self->clip_handler_id = 0;
  self->clip = gtk_widget_get_clipboard (GTK_WIDGET (self));

  /* Used e.g. by Xfce */
  gtk_window_set_default_icon_name (APP_ID);

  /* Prepare layout if we have initially RTL */
  if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    pt_window_direction_changed (GTK_WIDGET (self), GTK_TEXT_DIR_LTR);

  pt_window_ready_to_play (self, FALSE);

  self->clip_handler_id = g_signal_connect (self->clip,
                                            "changed",
                                            G_CALLBACK (update_insert_action_sensitivity),
                                            self);
  g_signal_connect (self->waveviewer,
                    "notify::follow-cursor",
                    G_CALLBACK (update_goto_cursor_action_sensitivity),
                    self);

  g_signal_connect (self->waveviewer,
                    "load-progress",
                    G_CALLBACK (progressbar_cb),
                    GTK_PROGRESS_BAR (self->progress));
}

static void
pt_window_dispose (GObject *object)
{
  PtWindow *self = PT_WINDOW (object);
  gint      x;
  gint      y;

  /* Save window size */
  if (self->editor)
    {
      gtk_window_get_default_size (GTK_WINDOW (self), &x, &y);
      g_settings_set_int (self->editor, "width", x);
      g_settings_set_int (self->editor, "height", y);
    }

  if (self->clip_handler_id > 0)
    {
      g_signal_handler_disconnect (self->clip, self->clip_handler_id);
      self->clip_handler_id = 0;
    }
  remove_timer (self);
  g_clear_object (&self->editor);
  g_clear_object (&self->player);
  g_clear_object (&self->asr_config);
  g_clear_object (&self->go_to_timestamp);
  g_clear_object (&self->vol_event);
  g_clear_object (&self->asr_menu);
  g_clear_object (&self->asr_menu_item1);
  g_clear_object (&self->asr_menu_item2);
  if (self->vol_icons)
    {
      g_strfreev (self->vol_icons);
      self->vol_icons = NULL;
    }

  G_OBJECT_CLASS (pt_window_parent_class)->dispose (object);
}

static void
pt_window_class_init (PtWindowClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = pt_window_dispose;
  widget_class->direction_changed = pt_window_direction_changed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/Parlatype/window.ui");
  gtk_widget_class_bind_template_callback (widget_class, play_button_toggled_cb);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, waveviewer);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, primary_menu_button);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, pos_menu_button);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, button_open);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, progress);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, controls_row_box);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, controls_box);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, button_play);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, button_jump_back);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, button_jump_forward);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, volumebutton);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, speed_scale);
}

PtWindow *
pt_window_new (PtApp *app)
{
  return g_object_new (PT_WINDOW_TYPE, "application", app, NULL);
}
