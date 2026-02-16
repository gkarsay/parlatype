/* Copyright 2016 Gabor Karsay <gabor.karsay@gmx.at>
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

#define G_LOG_DOMAIN "pt-window"
#define TEST_FILE "/home/gabor/dev/test-data/parlatype-test.pt"

#include "config.h"

#include "pt-window.h"

#include "editor-theme-selector-private.h"
#include "pt-app.h"
#include "pt-audio-controls.h"
#include "pt-goto-dialog.h"
#include "pt-save-changes-dialog.h"
#include "pt-utils.h"
#include "pt-window-dnd.h"
#include "pt-window-private.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <parlatype.h>
#include <stdlib.h> /* exit() */

struct _PtWindow
{
  AdwApplicationWindow parent_instance;
  PtPlayer            *player;
  GtkWidget           *waveviewer;
  GSettings           *editor;
  GtkLabel            *title;
  GtkLabel            *is_modified;
  GtkWidget           *primary_menu_button;

  PtAsrManager     *asr_manager;
  GtkRecentManager *recent;
  PtConfig         *asr_config;

  /* Main window widgets */
  GtkWidget *waveviewer_stack;
  GtkWidget *audio_controls;
  GtkWidget *progress;
  GtkWidget *timestamp_switch;

  GtkWidget *paned;
  GtkWidget *text_view;

  gint64 last_time; // last time to compare if it changed

  guint   timer;
  gdouble speed;
};

G_DEFINE_TYPE (PtWindow, pt_window, ADW_TYPE_APPLICATION_WINDOW)

static void
pt_error_message (PtWindow *parent,
                  gchar    *message)
{
  GtkAlertDialog *dialog;

  dialog = gtk_alert_dialog_new (_ ("Error"));
  gtk_alert_dialog_set_detail (dialog, message);
  gtk_alert_dialog_show (dialog, GTK_WINDOW (parent));
  g_object_unref (dialog);
}

static void
insert_timestamp (GtkWidget  *widget,
                  const char *action_name,
                  GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);

  pt_text_view_insert_timestamp (PT_TEXT_VIEW (self->text_view));
}

static void
goto_dialog_response_cb (GtkDialog *dlg,
                         int        response_id,
                         gpointer   user_data)
{
  PtWindow *self = PT_WINDOW (user_data);
  int       pos;

  if (response_id == GTK_RESPONSE_OK)
    {
      pos = pt_goto_dialog_get_pos (PT_GOTO_DIALOG (dlg));
      pt_player_jump_to_position (self->player, pos * 1000);
      pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
    }

  gtk_window_destroy (GTK_WINDOW (dlg));
}

static void
goto_position (GtkWidget  *widget,
               const char *action_name,
               GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);

  PtGotoDialog *dlg;

  dlg = pt_goto_dialog_new (GTK_WINDOW (self));
  pt_goto_dialog_set_pos (dlg, pt_player_get_position (self->player) / 1000);
  pt_goto_dialog_set_max (dlg, pt_player_get_duration (self->player) / 1000);

  g_signal_connect (dlg, "response",
                    G_CALLBACK (goto_dialog_response_cb), self);

  gtk_window_present (GTK_WINDOW (dlg));
}

static void
goto_cursor (GtkWidget  *widget,
             const char *action_name,
             GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);

  pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
}

static void
set_zoom (GSettings *editor,
          int        step)
{
  int pps;
  int new_pps;

  pps = g_settings_get_int (editor, "pps");
  if ((step > 0 && pps >= 200) || (step < 0 && pps <= 25))
    return;

  new_pps = (step == 0) ? 100 : CLAMP (pps + step, 25, 200);

  g_settings_set_int (editor, "pps", new_pps);
}

static void
zoom_in (GtkWidget  *widget,
         const char *action_name,
         GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);

  set_zoom (self->editor, 25);
}

static void
zoom_out (GtkWidget  *widget,
          const char *action_name,
          GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);
  set_zoom (self->editor, -25);
}

static void
zoom_default (GtkWidget  *widget,
              const char *action_name,
              GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);
  set_zoom (self->editor, 0);
}

static void
player_asr_hypothesis_cb (PtPlayer *player,
                          gchar    *word,
                          gpointer  user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  pt_text_view_insert_asr_hypothesis (PT_TEXT_VIEW (self->text_view), word);
}

static void
player_asr_final_cb (PtPlayer *player,
                     gchar    *word,
                     gpointer  user_data)
{
  PtWindow *self = PT_WINDOW (user_data);

  pt_text_view_insert_asr_final (PT_TEXT_VIEW (self->text_view), word);
}

static void
activate_cb (PtAsrManager *asr_manager,
             GAsyncResult *res,
             gpointer     *data)
{
  PtWindow *self = (PtWindow *) data;
  GError   *error = NULL;
  GAction  *action;

  if (!pt_asr_manager_activate_finish (asr_manager, res, &error))
    {
      pt_error_message (self, error->message);
      g_error_free (error);
      return;
    }

  if (pt_player_set_mode (self->player, PT_MODE_ASR))
    {
      pt_player_play (self->player);
      action = g_action_map_lookup_action (G_ACTION_MAP (self), "transcribe");
      g_action_change_state (G_ACTION (action), g_variant_new_boolean (TRUE));
    }
}

static void
set_mode_playback (PtWindow *self)
{
  pt_player_set_mode (self->player, PT_MODE_PLAYBACK);
}

void
pt_window_transcribe_action (GSimpleAction *action,
                             GVariant      *state,
                             gpointer       user_data)
{
  PtWindow *self = PT_WINDOW (user_data);
  GVariant *old_state, *new_state;
  gboolean  transcribe;

  old_state = g_action_get_state (G_ACTION (action));
  transcribe = !g_variant_get_boolean (old_state);
  new_state = g_variant_new_boolean (transcribe);

  if (transcribe)
    {
      pt_asr_manager_activate_async (self->asr_manager,
                                     self->asr_config,
                                     NULL,
                                     (GAsyncReadyCallback) activate_cb,
                                     self);
    }
  else
    {
      pt_player_pause (self->player);
      set_mode_playback (self);
      g_action_change_state (G_ACTION (action), new_state);
    }
}

static void
save_cb (GObject      *source_object,
         GAsyncResult *res,
         gpointer      user_data)
{
  PtWindow   *self = PT_WINDOW (user_data);
  PtTextView *text_view = PT_TEXT_VIEW (source_object);
  GError     *error = NULL;

  if (!pt_text_view_save_finish (text_view, res, &error))
    {
      pt_error_message (self, error->message);
      g_error_free (error);
    }
}

void
serialize (GtkWidget  *widget,
           const char *action_name,
           GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);
  GFile    *file;

  file = g_file_new_for_path (TEST_FILE);

  pt_text_view_save_async (PT_TEXT_VIEW (self->text_view),
                           file,
                           NULL,
                           save_cb,
                           self);
}

void
pt_window_save (PtWindow *self,
                GFile    *file)
{
  g_return_if_fail (PT_IS_WINDOW (self));
  g_return_if_fail (G_IS_FILE (file) || file == NULL);

  if (file == NULL)
    {
      file = (void *) pt_text_view_get_text_file (PT_TEXT_VIEW (self->text_view));
    }

  if (file == NULL)
    {
      pt_window_save_as (self);
      return;
    }

  pt_text_view_save_async (PT_TEXT_VIEW (self->text_view),
                           file,
                           NULL,
                           save_cb,
                           self);
}

static void
dialog_save_as_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  PtWindow *win = PT_WINDOW (user_data);
  GFile    *file;

  file = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), result, NULL);

  if (file == NULL)
    return;

  pt_window_save (win, file);
  g_object_unref (file);
}

void
pt_window_save_as (PtWindow *self)
{
  GtkFileDialog *dialog;
  const char    *home_path;
  GFile         *initial_folder;
  GListModel    *filters;

  g_return_if_fail (PT_IS_WINDOW (self));

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_modal (dialog, TRUE);

  gtk_file_dialog_set_title (dialog, _ ("Save as"));
  filters = pt_file_filters_new (PT_FILTER_TEXT, PT_FILTER_OPEN_DOCUMENT, PT_FILTER_ALL, 0);

  gtk_file_dialog_set_filters (dialog, filters);

  home_path = g_get_home_dir ();
  initial_folder = g_file_new_for_path (home_path);
  gtk_file_dialog_set_initial_folder (dialog, initial_folder);

  gtk_file_dialog_save (dialog, GTK_WINDOW (self),
                        NULL,
                        dialog_save_as_cb,
                        self);

  g_object_unref (initial_folder);
  g_object_unref (dialog);
}

/*static void
load_cb (GObject      *source_object,
         GAsyncResult *res,
         gpointer      user_data)
{
  PtWindow   *self = PT_WINDOW (user_data);
  PtTextView *text_view = PT_TEXT_VIEW (source_object);
  GError     *error = NULL;

  if (!pt_text_view_load_finish (text_view, res, &error))
    {
      pt_error_message (self, error->message);
      g_error_free (error);
    }
}*/

void
deserialize (GtkWidget  *widget,
             const char *action_name,
             GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);
  GError   *error = NULL;

  GFile *file;

  file = g_file_new_for_path (TEST_FILE);

  if (!pt_text_view_load (PT_TEXT_VIEW (self->text_view), file, &error))
    {
      pt_error_message (self, error->message);
      g_error_free (error);
    }
}

void
pt_window_load_document (PtWindow *self,
                         GFile    *file)
{
  GError *error = NULL;

  g_return_if_fail (PT_IS_WINDOW (self));
  g_return_if_fail (G_IS_FILE (file));

  if (!pt_text_view_load (PT_TEXT_VIEW (self->text_view), file, &error))
    {
      pt_error_message (self, error->message);
      g_error_free (error);
    }
}

void
pt_window_save_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  PtWindow *self = PT_WINDOW (widget);

  if (g_strcmp0 (action_name, "win.save") == 0)
    pt_window_save (self, NULL);
  else if (g_strcmp0 (action_name, "win.save-as") == 0)
    pt_window_save_as (self);
  else
    g_assert_not_reached ();
}

const GActionEntry win_actions[] = {
  { "transcribe", pt_window_transcribe_action, NULL, "false", NULL },
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

      pt_audio_controls_set_time_label (PT_AUDIO_CONTROLS (self->audio_controls), text);
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
update_goto_cursor_action_sensitivity (GObject    *gobject,
                                       GParamSpec *pspec,
                                       gpointer    user_data)
{
  PtWaveviewer *viewer = PT_WAVEVIEWER (gobject);
  PtWindow     *self = PT_WINDOW (user_data);
  gboolean      follow;

  follow = pt_waveviewer_get_follow_cursor (viewer);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.goto-cursor", !follow);
}

static void
player_error_cb (PtPlayer *player,
                 GError   *error,
                 PtWindow *self)
{
  pt_error_message (self, error->message);
  /* TODO when in ASR mode, reset mode and toggle button */
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
      pt_error_message (self, error->message);
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

  gchar    *current_uri;
  GFile    *audio_file;
  int       cmp = 1;
  GtkStack *stack = GTK_STACK (self->waveviewer_stack);

  /* Don't reload already loaded waveform */
  current_uri = pt_player_get_uri (self->player);
  if (current_uri)
    {
      cmp = g_strcmp0 (current_uri, uri);
      g_free (current_uri);
    }
  if (cmp == 0)
    return;

  gtk_stack_set_visible_child (stack, gtk_stack_get_child_by_name (stack, "waveviewer"));
  if (!pt_player_open_uri (self->player, uri))
    {
      gtk_stack_set_visible_child (stack, gtk_stack_get_child_by_name (stack, "empty"));
      return;
    }

  audio_file = g_file_new_for_uri (uri);
  pt_text_view_set_audio_file (PT_TEXT_VIEW (self->text_view), audio_file);
  g_object_unref (audio_file);

  pt_waveviewer_load_wave_async (PT_WAVEVIEWER (self->waveviewer),
                                 uri,
                                 NULL,
                                 (GAsyncReadyCallback) open_cb,
                                 self);
}

static void
player_state_changed_cb (PtPlayer   *player,
                         GParamSpec *param,
                         PtWindow   *self)
{
  PtStateType player_state;
  GAction    *transcribe_action;
  gboolean    ready, playing, transcribing;
  GVariant   *action_state;

  player_state = pt_player_get_state (player);
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "player state changed: %s", pt_state_get_name (player_state));

  ready = player_state >= PT_STATE_PAUSED;
  playing = player_state == PT_STATE_PLAYING;
  transcribing = player_state == PT_STATE_TRANSCRIBING;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.insert-timestamp", ready);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.goto", ready);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-in", ready);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-out", ready);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.zoom-default", ready);
  /* always insensitive: either there is no waveform or we are already at cursor position */
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.goto-cursor", FALSE);

  transcribe_action = g_action_map_lookup_action (G_ACTION_MAP (self), "transcribe");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (transcribe_action),
                               ready && !playing && self->asr_config != NULL);
  action_state = g_variant_new_boolean (transcribing);
  g_action_change_state (transcribe_action, action_state);

  if (ready)
    {
      pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (self->waveviewer), TRUE);
      if (self->timer == 0)
        {
          add_timer (self);
        }
    }
  else
    {
      pt_audio_controls_set_time_label (PT_AUDIO_CONTROLS (self->audio_controls), NULL);
      remove_timer (self);
    }
}

static void
player_end_of_stream_cb (PtPlayer *player,
                         PtWindow *self)
{
  /* Pause player without jumping back, this will also toggle the
   * Play button */
  pt_player_pause (self->player);
}

static void
player_uri_changed_cb (PtPlayer   *player,
                       GParamSpec *pspec,
                       PtWindow   *self)
{
  char *uri = NULL;
  char *display_name;

  display_name = pt_player_get_filename (self->player);
  if (display_name)
    {
      gtk_label_set_text (self->title, display_name);
      g_free (display_name);
    }
  else
    {
      gtk_label_set_text (self->title, _ ("Parlatype"));
    }

  uri = pt_player_get_uri (self->player);
  if (uri)
    {
      gtk_recent_manager_add_item (self->recent, uri);
      g_free (uri);
    }
}

/*
 * Called on startup and on config file changes
 */
static void
set_asr_config (PtWindow *self)
{
  GFile   *asr_file;
  gchar   *asr_path;
  GAction *action;
  gboolean enable;

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "transcribe");

  /* Clear old config object and recreate with new config file */
  if (self->asr_config)
    g_clear_object (&self->asr_config);

  asr_path = g_settings_get_string (self->editor, "asr-config");
  asr_file = g_file_new_for_path (asr_path);
  self->asr_config = pt_config_new (asr_file);
  g_object_unref (asr_file);

  enable = pt_config_is_valid (self->asr_config) &&
           pt_asr_manager_plugin_is_available (self->asr_manager, self->asr_config) &&
           pt_config_is_installed (self->asr_config);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enable);
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "asr %s, config path: »%s«", enable ? "enabled" : "disabled", asr_path);

  if (!enable)
    {
      g_clear_object (&self->asr_config);
    }

  g_free (asr_path);
}

static void
settings_changed_cb (GSettings *settings,
                     gchar     *key,
                     PtWindow  *self)
{
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
  int new;
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
  int new;
  new = g_value_get_int (value);
  new = new / 1000;
  return g_variant_new_int32 (new);
}

static void
setup_settings (PtWindow *self)
{
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

  set_asr_config (self);

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

  gtk_paned_set_position (GTK_PANED (self->paned),
                          g_settings_get_int (self->editor, "separator"));
}

static void
setup_player (PtWindow *self)
{
  self->player = pt_player_new ();

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
                    "notify::state",
                    G_CALLBACK (player_state_changed_cb),
                    self);

  g_signal_connect (self->player,
                    "notify::current-uri",
                    G_CALLBACK (player_uri_changed_cb),
                    self);

  g_signal_connect (self->player,
                    "asr-final",
                    G_CALLBACK (player_asr_final_cb),
                    self);

  g_signal_connect (self->player,
                    "asr-hypothesis",
                    G_CALLBACK (player_asr_hypothesis_cb),
                    self);
}

static void
setup_accels_actions_menus (PtWindow *self)
{
  /* Actions */
  GSimpleActionGroup *group = g_simple_action_group_new ();
  GAction            *action = g_settings_create_action (self->editor, "style-variant");
  g_action_map_add_action (G_ACTION_MAP (group), action);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "settings",
                                  G_ACTION_GROUP (group));
  g_object_unref (action);
  g_object_unref (group);

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   win_actions,
                                   G_N_ELEMENTS (win_actions),
                                   self);
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

PtAsrManager *
_pt_window_get_asr_manager (PtWindow *self)
{
  return self->asr_manager;
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

static gboolean
f10_ctrl_cb (GtkEventControllerKey *self,
             guint                  keyval,
             guint                  keycode,
             GdkModifierType        state,
             gpointer               user_data)
{
  GtkMenuButton *button = GTK_MENU_BUTTON (user_data);
  gboolean       modifier_pressed;

  modifier_pressed = state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK);
  if (modifier_pressed || keyval != GDK_KEY_F10)
    return FALSE;

  gtk_menu_button_popdown (button);
  return TRUE;
}

static void
pt_window_confirm_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  PtWindow *self = PT_WINDOW (user_data);
  GError   *error = NULL;

  if (pt_save_changes_dialog_run_finish (result, &error))
    {
      /* TODO cancel cancellable */
      gtk_window_destroy (GTK_WINDOW (self));
    }
  else
    {
      /* show error messages except user cancelled operation */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          pt_error_message (self, error->message);
        }
      g_error_free (error);
    }
}

static gboolean
pt_window_close_request (GtkWindow *window)
{
  PtWindow *self = PT_WINDOW (window);
  int       x, y;

  /* Save window size and separator position */
  gtk_window_get_default_size (GTK_WINDOW (self), &x, &y);
  g_settings_set_int (self->editor, "width", x);
  g_settings_set_int (self->editor, "height", y);
  g_settings_set_int (self->editor, "separator", gtk_paned_get_position (GTK_PANED (self->paned)));

  if (pt_text_view_get_is_modified (PT_TEXT_VIEW (self->text_view)))
    {
      pt_save_changes_dialog_run_async (GTK_WINDOW (self),
                                        PT_TEXT_VIEW (self->text_view),
                                        NULL,
                                        pt_window_confirm_cb,
                                        self);
      return TRUE;
    }

  /* TODO cancel cancellable */

  return GTK_WINDOW_CLASS (pt_window_parent_class)->close_request (window);
}

static void
pt_window_dispose (GObject *object)
{
  PtWindow *self = PT_WINDOW (object);

  remove_timer (self);
  g_clear_object (&self->editor);
  g_clear_object (&self->player);
  g_clear_object (&self->asr_manager);
  g_clear_object (&self->asr_config);

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_WINDOW);

  G_OBJECT_CLASS (pt_window_parent_class)->dispose (object);
}

static void
pt_window_constructed (GObject *object)
{
  PtWindow           *self = PT_WINDOW (object);
  GtkPopover         *popover;
  GtkMenuButton      *button;
  GtkEventController *f10_ctrl;

  G_OBJECT_CLASS (pt_window_parent_class)->constructed (object);

  /* Add theme selector to main menu. */
  button = GTK_MENU_BUTTON (self->primary_menu_button);
  popover = gtk_menu_button_get_popover (button);
  gtk_popover_menu_add_child (GTK_POPOVER_MENU (popover),
                              _editor_theme_selector_new (),
                              "theme");

  /* Make F10 toggle main menu. Popup is realized with setting property "primary"
   * in ui-file, popdown with the following code. */
  f10_ctrl = gtk_event_controller_key_new ();
  g_signal_connect (f10_ctrl, "key-pressed",
                    G_CALLBACK (f10_ctrl_cb), button);
  gtk_widget_add_controller (GTK_WIDGET (popover), f10_ctrl);
}

static void
audio_file_cb (PtTextView *text_view,
               GParamSpec *pspec,
               gpointer    user_data)
{
  PtWindow    *self = PT_WINDOW (user_data);
  const GFile *audio_file;
  char        *uri;

  audio_file = pt_text_view_get_audio_file (text_view);
  if (audio_file == NULL)
    return;

  uri = g_file_get_uri ((void *) audio_file);
  pt_window_open_file (self, uri);
  g_free (uri);
}

static void
pt_window_init (PtWindow *self)
{
  g_type_ensure (PT_TYPE_TEXT_VIEW);
  gtk_widget_init_template (GTK_WIDGET (self));

  self->editor = g_settings_new (APP_ID);
  setup_player (self);
  pt_audio_controls_set_player (PT_AUDIO_CONTROLS (self->audio_controls), self->player);
  pt_text_view_connect (PT_TEXT_VIEW (self->text_view),
                        self->player,
                        self->editor);
  self->asr_manager = pt_asr_manager_new (self->player, PT_TEXT_VIEW (self->text_view));

  setup_accels_actions_menus (self);
  setup_settings (self);
  pt_window_setup_dnd (self); /* this is in pt_window_dnd.c */
  player_state_changed_cb (self->player, NULL, self);

  g_object_bind_property (
      self->text_view, "timestamps-visible",
      self->timestamp_switch, "active",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  g_object_bind_property (
      self->text_view, "is-modified",
      self->is_modified, "visible",
      G_BINDING_DEFAULT);

  g_signal_connect (self->text_view, "notify::audio", G_CALLBACK (audio_file_cb), self);

  self->recent = gtk_recent_manager_get_default ();
  self->timer = 0;
  self->last_time = 0;

  /* Used e.g. by Xfce */
  gtk_window_set_default_icon_name (APP_ID);

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
pt_window_class_init (PtWindowClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  gobject_class->constructed = pt_window_constructed;
  gobject_class->dispose = pt_window_dispose;
  window_class->close_request = pt_window_close_request;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/window.ui");

  gtk_widget_class_bind_template_child (widget_class, PtWindow, waveviewer);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, primary_menu_button);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, progress);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, audio_controls);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, is_modified);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, paned);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, text_view);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, title);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, waveviewer_stack);
  gtk_widget_class_bind_template_child (widget_class, PtWindow, timestamp_switch);

  gtk_widget_class_install_action (widget_class, "win.goto", NULL, goto_position);
  gtk_widget_class_install_action (widget_class, "win.goto-cursor", NULL, goto_cursor);
  gtk_widget_class_install_action (widget_class, "win.insert-timestamp", NULL, insert_timestamp);
  gtk_widget_class_install_action (widget_class, "win.save", NULL, pt_window_save_action);
  gtk_widget_class_install_action (widget_class, "win.save-as", NULL, pt_window_save_action);
  gtk_widget_class_install_action (widget_class, "win.zoom-in", NULL, zoom_in);
  gtk_widget_class_install_action (widget_class, "win.zoom-out", NULL, zoom_out);
  gtk_widget_class_install_action (widget_class, "win.zoom-default", NULL, zoom_default);
  gtk_widget_class_install_action (widget_class, "win.serialize", NULL, serialize);
  gtk_widget_class_install_action (widget_class, "win.deserialize", NULL, deserialize);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_g, GDK_CONTROL_MASK, "win.goto", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_s, GDK_CONTROL_MASK, "win.save", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_s, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "win.save-as", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_t, GDK_CONTROL_MASK, "win.insert-timestamp", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_plus, GDK_CONTROL_MASK, "win.zoom-in", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Add, GDK_CONTROL_MASK, "win.zoom-in", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_minus, GDK_CONTROL_MASK, "win.zoom-out", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Subtract, GDK_CONTROL_MASK, "win.zoom-out", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_0, GDK_CONTROL_MASK, "win.zoom-default", NULL);
}

PtWindow *
pt_window_new (PtApp *app)
{
  return g_object_new (PT_TYPE_WINDOW, "application", app, NULL);
}
