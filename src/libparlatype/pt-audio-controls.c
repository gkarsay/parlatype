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
 * PtAudioControls:
 *
 * A bin with audio controls.
 */

#include "config.h"

#include "pt-audio-controls.h"

#include "pt-playback-rate-button.h"
#include "pt-volume-button.h"

#include <glib/gi18n.h>

struct _PtAudioControls
{
  AdwBin parent_instance;

  PtPlayer *player;

  GActionGroup *actions;

  GtkWidget *playback_rate_button;
  GtkWidget *volume_button;
  GtkWidget *pos_menu_button;
  GtkWidget *controls_box;
  GtkWidget *button_jump_back;
  GtkWidget *button_jump_forward;
  GtkWidget *button_play;
};

G_DEFINE_FINAL_TYPE (PtAudioControls, pt_audio_controls, ADW_TYPE_BIN)

static void
pt_audio_controls_direction_changed (GtkWidget       *widget,
                                     GtkTextDirection previous_direction)
{
  /* In RTL layouts playback control elements are *not* supposed to be
   * mirrored. Undo automatic mirroring for those elements. */

  PtAudioControls *self = PT_AUDIO_CONTROLS (widget);

  gtk_widget_set_direction (self->button_jump_back, GTK_TEXT_DIR_LTR);
  gtk_widget_set_direction (self->button_jump_forward, GTK_TEXT_DIR_LTR);
  gtk_widget_set_direction (self->controls_box, GTK_TEXT_DIR_LTR);
}

static void
update_jump_back_tooltip (PtAudioControls *self)
{
  gchar *tooltip = NULL;
  int    seconds;

  if (pt_player_get_state (self->player) >= PT_STATE_PAUSED)
    {
      seconds = pt_player_get_back (self->player) / 1000;
      tooltip = g_strdup_printf (
          /* Translators: tooltip for a button */
          ngettext ("Skip back 1 second",
                    "Skip back %d seconds",
                    seconds),
          seconds);
    }

  gtk_widget_set_tooltip_text (self->button_jump_back, tooltip);
  g_free (tooltip);
}

static void
update_jump_forward_tooltip (PtAudioControls *self)
{
  gchar *tooltip = NULL;
  int    seconds;

  if (pt_player_get_state (self->player) >= PT_STATE_PAUSED)
    {
      seconds = pt_player_get_forward (self->player) / 1000;
      tooltip = g_strdup_printf (
          /* Translators: tooltip for a button */
          ngettext ("Skip forward 1 second",
                    "Skip forward %d seconds",
                    seconds),
          seconds);
    }

  gtk_widget_set_tooltip_text (self->button_jump_forward, tooltip);
  g_free (tooltip);
}

static void
update_play_button_tooltip (PtAudioControls *self)
{
  gchar      *tooltip = NULL;
  int         pause;
  gboolean    free_me = FALSE;
  PtStateType state;

  state = pt_player_get_state (self->player);

  if (state >= PT_STATE_PAUSED)
    {
      if (state == PT_STATE_PAUSED)
        {
          /* Translators: tooltip for play button */
          tooltip = _ ("Start playing");
        }
      if (state == PT_STATE_PLAYING)
        {
          pause = pt_player_get_pause (self->player) / 1000;
          if (pause == 0)
            {
              /* Translators: tooltip for pause button */
              tooltip = _ ("Pause");
            }
          else
            {
              tooltip = g_strdup_printf (
                  /* Translators: tooltip for pause button */
                  ngettext ("Pause and rewind 1 second",
                            "Pause and rewind %d seconds",
                            pause),
                  pause);
              free_me = TRUE;
            }
        }
    }

  gtk_widget_set_tooltip_text (self->button_play, tooltip);
  if (free_me)
    g_free (tooltip);
}

static void
pt_audio_controls_play_action (GSimpleAction *action,
                               GVariant      *state,
                               gpointer       user_data)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (user_data);

  pt_player_play_pause (self->player);
}

static void
pt_audio_controls_jump_back_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (widget);

  pt_player_jump_back (self->player);
}

static void
pt_audio_controls_jump_forward_action (GtkWidget  *widget,
                                       const char *action_name,
                                       GVariant   *param)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (widget);

  pt_player_jump_forward (self->player);
}

const GActionEntry audio_actions[] = {
  { "play", pt_audio_controls_play_action, NULL, "true", NULL },
};

static void
player_notify_back_cb (PtPlayer   *player,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (user_data);
  update_jump_back_tooltip (self);
}

static void
player_notify_forward_cb (PtPlayer   *player,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (user_data);
  update_jump_forward_tooltip (self);
}

static void
player_notify_pause_cb (PtPlayer   *player,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (user_data);
  update_play_button_tooltip (self);
}

static void
player_notify_state_cb (PtPlayer   *player,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (user_data);
  GAction         *play_action;
  GVariant        *action_state;
  PtStateType      state;

  update_jump_back_tooltip (self);
  update_jump_forward_tooltip (self);
  update_play_button_tooltip (self);

  state = pt_player_get_state (self->player);
  gtk_widget_set_sensitive (self->volume_button, state >= PT_STATE_PAUSED);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "audio.jump-back", state >= PT_STATE_PAUSED);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "audio.jump-forward", state >= PT_STATE_PAUSED);

  play_action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), "play");
  g_assert (G_IS_SIMPLE_ACTION (play_action));
  g_simple_action_set_enabled (G_SIMPLE_ACTION (play_action), state == PT_STATE_PAUSED || state == PT_STATE_PLAYING);
  action_state = g_variant_new_boolean (state == PT_STATE_PLAYING);
  g_action_change_state (play_action, action_state);
}

/**
 * pt_audio_controls_set_time_label:
 * @self: the controls bin
 * @label: text for the time label (nullable)
 *
 * Sets the time label. There are no checks on the format or content of
 * the time label. If @label is NULL, it is set to "00:00.0".
 */
void
pt_audio_controls_set_time_label (PtAudioControls *self,
                                  char            *label)
{
  g_return_if_fail (PT_IS_AUDIO_CONTROLS (self));

  if (label == NULL)
    {
      label = "00:00.0";
    }

  gtk_menu_button_set_label (GTK_MENU_BUTTON (self->pos_menu_button), label);
}

/**
 * pt_audio_controls_set_player:
 * @self: the controls bin
 * @player: a `PtPlayer`
 *
 * The widgets will use @player to control playback and react to the various
 * states of @player. An exception is the time label which has to be updated
 * manually.
 */
void
pt_audio_controls_set_player (PtAudioControls *self,
                              PtPlayer        *player)
{
  g_return_if_fail (PT_IS_AUDIO_CONTROLS (self));
  g_return_if_fail (PT_IS_PLAYER (player));

  self->player = player;

  pt_volume_button_set_player (PT_VOLUME_BUTTON (self->volume_button), self->player);

  GtkAdjustment *rate_adjustment;
  rate_adjustment = pt_playback_rate_button_get_adjustment (PT_PLAYBACK_RATE_BUTTON (self->playback_rate_button));
  g_object_bind_property (
      rate_adjustment, "value",
      self->player, "speed",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  g_signal_connect (self->player,
                    "notify::state",
                    G_CALLBACK (player_notify_state_cb),
                    self);
  g_signal_connect (self->player,
                    "notify::back",
                    G_CALLBACK (player_notify_back_cb),
                    self);
  g_signal_connect (self->player,
                    "notify::forward",
                    G_CALLBACK (player_notify_forward_cb),
                    self);
  g_signal_connect (self->player,
                    "notify::pause",
                    G_CALLBACK (player_notify_pause_cb),
                    self);

  player_notify_state_cb (self->player, NULL, self);
}

static void
pt_audio_controls_dispose (GObject *object)
{
  PtAudioControls *self = PT_AUDIO_CONTROLS (object);

  g_clear_object (&self->actions);
  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_AUDIO_CONTROLS);

  G_OBJECT_CLASS (pt_audio_controls_parent_class)->dispose (object);
}

static void
pt_audio_controls_init (PtAudioControls *self)
{
  g_type_ensure (PT_TYPE_PLAYBACK_RATE_BUTTON);
  g_type_ensure (PT_TYPE_VOLUME_BUTTON);
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Prepare layout if we have initially RTL */
  if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    {
      pt_audio_controls_direction_changed (GTK_WIDGET (self), GTK_TEXT_DIR_LTR);
    }

  self->actions = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   audio_actions,
                                   G_N_ELEMENTS (audio_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "audio", self->actions);
}

static void
pt_audio_controls_class_init (PtAudioControlsClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_audio_controls_dispose;
  widget_class->direction_changed = pt_audio_controls_direction_changed;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/audio-controls.ui");
  gtk_widget_class_bind_template_child (widget_class, PtAudioControls, playback_rate_button);
  gtk_widget_class_bind_template_child (widget_class, PtAudioControls, volume_button);
  gtk_widget_class_bind_template_child (widget_class, PtAudioControls, pos_menu_button);
  gtk_widget_class_bind_template_child (widget_class, PtAudioControls, controls_box);
  gtk_widget_class_bind_template_child (widget_class, PtAudioControls, button_jump_back);
  gtk_widget_class_bind_template_child (widget_class, PtAudioControls, button_jump_forward);
  gtk_widget_class_bind_template_child (widget_class, PtAudioControls, button_play);

  gtk_widget_class_install_action (widget_class, "audio.jump-back", NULL, pt_audio_controls_jump_back_action);
  gtk_widget_class_install_action (widget_class, "audio.jump-forward", NULL, pt_audio_controls_jump_forward_action);
}

/**
 * pt_audio_controls_new:
 *
 * Returns a new bin with audio controls.
 *
 * Returns: a newly created `PtAudioControls`.
 */
PtAudioControls *
pt_audio_controls_new (void)
{
  return g_object_new (PT_TYPE_AUDIO_CONTROLS, NULL);
}
