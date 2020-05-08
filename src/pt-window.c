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
#include <stdlib.h>		/* exit() */
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <pt-player.h>
#include <pt-waveviewer.h>
#include "pt-app.h"
#include "pt-asr-assistant.h"
#include "pt-goto-dialog.h"
#include "pt-window.h"
#include "pt-window-dnd.h"
#include "pt-window-private.h"


G_DEFINE_TYPE_WITH_PRIVATE (PtWindow, pt_window, GTK_TYPE_APPLICATION_WINDOW)

static void play_button_toggled_cb (GtkToggleButton *button, PtWindow *win);
static void jump_back_button_clicked_cb (GtkButton *button, PtWindow *win);
static void jump_forward_button_clicked_cb (GtkButton *button, PtWindow *win);

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

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

void
copy_timestamp (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
	PtWindow *win;
	win = PT_WINDOW (user_data);
	const gchar  *timestamp = NULL;

	timestamp = pt_player_get_timestamp (win->player);
	if (timestamp)
		gtk_clipboard_set_text (win->priv->clip, timestamp, -1);
}

static void
clip_text_cb (GtkClipboard *clip,
              const gchar  *text,
              gpointer      data)
{
	PtWindow *win = (PtWindow *) data;
	gchar *timestamp;

	if (text) {
		timestamp = g_strdup (text);
		pt_player_goto_timestamp (win->player, timestamp);
		pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
		g_free (timestamp);
	}
}

void
insert_timestamp (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
	PtWindow *win;
	win = PT_WINDOW (user_data);

	gtk_clipboard_request_text (win->priv->clip, clip_text_cb, win);
}

void
goto_position (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
	PtWindow *win;
	win = PT_WINDOW (user_data);

	PtGotoDialog *dlg;
	gint	      pos;

	dlg = pt_goto_dialog_new (GTK_WINDOW (win));
	pt_goto_dialog_set_pos (dlg, pt_player_get_position (win->player) / 1000);
	pt_goto_dialog_set_max (dlg, pt_player_get_duration (win->player) / 1000);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK) {
		pos = pt_goto_dialog_get_pos (dlg);
		pt_player_jump_to_position (win->player, pos * 1000);
		pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
	}

	gtk_widget_destroy (GTK_WIDGET (dlg));
}

void
goto_cursor (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
	PtWindow *win;
	win = PT_WINDOW (user_data);

	pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
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
	PtWindow *win = PT_WINDOW (user_data);
	set_zoom (win->priv->editor, 25);
}

void
zoom_out (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
	PtWindow *win = PT_WINDOW (user_data);
	set_zoom (win->priv->editor, -25);
}

#ifdef HAVE_ASR
static void
setup_sphinx (PtWindow *win)
{
	GError *error = NULL;

	pt_asr_settings_apply_config (
			win->priv->asr_settings,
			g_settings_get_string (win->priv->editor, "asr-config"),
			win->player);

	pt_player_setup_sphinx (win->player, &error);
	if (error) {
		pt_error_message (win, error->message, NULL);
		g_clear_error (&error);
	}
}

static void
asr_output_search_cancelled_cb (PtAsrOutput *output,
                                PtWindow    *win)
{
	GAction  *action;
	GVariant *state;

	g_signal_handler_disconnect (win->priv->output, win->priv->output_handler_id1);
	g_signal_handler_disconnect (win->priv->output, win->priv->output_handler_id2);
	g_clear_object (&win->priv->output);

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "mode");
	state = g_variant_new_string ("playback");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), state);
}

static void
asr_output_app_found_cb (PtAsrOutput *output,
                         PtWindow    *win)
{
	GApplication  *app;
	GNotification *notification;
	gchar         *summary;
	gchar         *message;

	g_signal_handler_disconnect (win->priv->output, win->priv->output_handler_id1);
	g_signal_handler_disconnect (win->priv->output, win->priv->output_handler_id2);

	summary = g_strdup_printf (_("Found %s"), pt_asr_output_get_app_name (output));
	message = _("Press “Play” to start automatic speech recognition.");
	notification = g_notification_new (summary);
	g_notification_set_body (notification, message);
	app = G_APPLICATION (gtk_window_get_application (GTK_WINDOW (win)));
	g_application_send_notification (app, "app-found", notification);

	setup_sphinx (win);

	g_application_withdraw_notification (app, "app-found");
	g_object_unref (notification);
	g_free (summary);
}

static void
set_mode_asr (PtWindow *win)
{
	win->priv->output = pt_asr_output_new ();
	win->priv->output_handler_id1 = g_signal_connect (
			win->priv->output, "app-found",
			G_CALLBACK (asr_output_app_found_cb), win);
	win->priv->output_handler_id2 = g_signal_connect (
			win->priv->output, "search-cancelled",
			G_CALLBACK (asr_output_search_cancelled_cb), win);
	pt_asr_output_search_app (win->priv->output, GTK_WINDOW (win));
}

static void
asr_assistant_cancel_cb (GtkAssistant *assistant,
                         gpointer      user_data)
{
	gtk_widget_destroy (GTK_WIDGET (assistant));
}

static void
asr_assistant_close_cb (GtkAssistant *assistant,
                        PtWindow     *win)
{
	gchar            *name;
	gchar            *id;

	name = pt_asr_assistant_get_name (PT_ASR_ASSISTANT (assistant));
	id = pt_asr_assistant_save_config (
			PT_ASR_ASSISTANT (assistant), win->priv->asr_settings);
	g_settings_set_string (win->priv->editor, "asr-config", id);

	gtk_widget_destroy (GTK_WIDGET (assistant));
	g_free (name);
	g_free (id);
}

static void
launch_asr_assistant (PtWindow *win)
{
	GtkWidget *assistant;

	assistant = GTK_WIDGET (pt_asr_assistant_new (GTK_WINDOW (win)));
	g_signal_connect (assistant, "cancel", G_CALLBACK (asr_assistant_cancel_cb), NULL);
	g_signal_connect (assistant, "close", G_CALLBACK (asr_assistant_close_cb), win);
	gtk_widget_show (assistant);
}

static void
set_mode_playback (PtWindow *win)
{
	GError *error = NULL;

	pt_player_setup_player (win->player, &error);
	if (error) {
		pt_error_message (win, error->message, NULL);
		g_clear_error (&error);
	}
}

void
change_mode (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
	PtWindow *win = PT_WINDOW (user_data);
	const gchar *mode;

	if (!win->priv->asr_settings) {
		GtkApplication *app;
		app = gtk_window_get_application (GTK_WINDOW (win));
		win->priv->asr_settings = g_object_ref (pt_app_get_asr_settings (PT_APP (app)));
	}

	mode = g_variant_get_string (state, NULL);
	if (g_strcmp0 (mode, "playback") == 0) {
		g_clear_object (&win->priv->output);
		set_mode_playback (win);
	} else if (g_strcmp0 (mode, "asr") == 0) {
		if (pt_asr_settings_have_configs (win->priv->asr_settings)) {
			/* TODO && have config saved in GSettings */
			set_mode_asr (win);
		} else {
			launch_asr_assistant (win);
			return;
		}
	} else {
		g_assert_not_reached ();
	}

	g_simple_action_set_state (action, state);

	/* On changing state the menu doesn’t close. It seems better to close
	   it just like every other menu item closes the menu.
	   Just in case this behaviour changes in the future, check if the
	   menu is really open. */

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->primary_menu_button)))
		gtk_button_clicked (GTK_BUTTON (win->priv->primary_menu_button));
}
#endif

const GActionEntry win_actions[] = {
#ifdef HAVE_ASR
	{ "mode", NULL, "s", "'playback'", change_mode },
#endif
	{ "copy", copy_timestamp, NULL, NULL, NULL },
	{ "insert", insert_timestamp, NULL, NULL, NULL },
	{ "goto", goto_position, NULL, NULL, NULL },
	{ "goto-cursor", goto_cursor, NULL, NULL, NULL },
	{ "zoom-in", zoom_in, NULL, NULL, NULL },
	{ "zoom-out", zoom_out, NULL, NULL, NULL }
};

static void
update_time (PtWindow *win)
{
	gchar *text;
	gint64 time;

	time = pt_player_get_position (win->player);

	/* Update label only if time has changed at 10th seconds level */
	if (time/100 != win->priv->last_time) {
		text = pt_player_get_current_time_string (
				win->player,
				PT_PRECISION_SECOND_10TH);

		if (text == NULL)
			return;

		gtk_label_set_text (GTK_LABEL (win->priv->pos_label), text);
		g_free (text);
		win->priv->last_time = time/100;
	}

	g_object_set (win->priv->waveviewer, "playback-cursor", time, NULL);
}

static gboolean
update_time_tick (GtkWidget     *widget,
	                GdkFrameClock *frame_clock,
	                gpointer       data)
{
	PtWindow *win = (PtWindow *) data;
	update_time (win);
	return G_SOURCE_CONTINUE;
}

static void
zoom_in_cb (GtkWidget *widget,
            PtWindow  *win)
{
	GAction  *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "zoom-in");
	g_action_activate (action, NULL);
}

static void
zoom_out_cb (GtkWidget *widget,
             PtWindow  *win)
{
	GAction  *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "zoom-out");
	g_action_activate (action, NULL);
}

static void
add_timer (PtWindow *win)
{
	if (win->priv->timer == 0) {
		win->priv->timer = gtk_widget_add_tick_callback (
						win->priv->waveviewer,
						update_time_tick,
						win,
						NULL);
	}
}

static void
remove_timer (PtWindow *win)
{
	if (win->priv->timer > 0) {
		gtk_widget_remove_tick_callback (win->priv->waveviewer,
						 win->priv->timer);
		win->priv->timer = 0;
	}
}

static void
change_jump_back_tooltip (PtWindow *win)
{
	gchar *back;
	gint   seconds;

	seconds = pt_player_get_back (win->player) / 1000;
	back = g_strdup_printf (
			ngettext ("Jump back 1 second",
				  "Jump back %d seconds",
				  seconds),
			seconds);

	gtk_widget_set_tooltip_text (win->priv->button_jump_back, back);
	g_free (back);
}

static void
change_jump_forward_tooltip (PtWindow *win)
{
	gchar *forward;
	gint   seconds;

	seconds = pt_player_get_forward (win->player) / 1000;
	forward = g_strdup_printf (
			ngettext ("Jump forward 1 second",
				  "Jump forward %d seconds",
				  seconds),
			seconds);

	gtk_widget_set_tooltip_text (win->priv->button_jump_forward, forward);
	g_free (forward);
}

static void
change_play_button_tooltip (PtWindow *win)
{
	gchar    *play;
	gint      pause;
	gboolean  free_me = FALSE;

	pause = pt_player_get_pause (win->player) / 1000;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play))) {
		if (pause == 0) {
			play = _("Pause");
		} else {
			play = g_strdup_printf (
					ngettext ("Pause and rewind 1 second",
						  "Pause and rewind %d seconds",
						  pause),
					pause);
			free_me = TRUE;
		}
	} else {
		play = _("Start playing");
	}

	gtk_widget_set_tooltip_text (win->priv->button_play, play);
	if (free_me)
		g_free (play);
}

static void
update_insert_action_sensitivity_cb (GtkClipboard *clip,
                                     const gchar  *text,
                                     gpointer      data)
{
	PtWindow *win = PT_WINDOW (data);
	PtPlayer *player = win->player;
	gchar    *timestamp = NULL;
	gboolean  result = FALSE;
	GAction  *action;
	gint      pos;
	gchar    *label;

	if (text) {
		timestamp = g_strdup (text);
		pos = pt_player_get_timestamp_position (player, timestamp, TRUE);
		g_free (timestamp);
		timestamp = NULL;
		if (pos >= 0) {
			result = TRUE;
			timestamp = pt_player_get_time_string (
					pos,
					pt_player_get_duration (player),
					PT_PRECISION_SECOND_10TH);
		}
	}

	if (timestamp)
		label = g_strdup_printf (_("Go to time in clipboard: %s"), timestamp);
	else
		label = g_strdup (_("Go to time in clipboard"));

	g_menu_item_set_label (win->priv->go_to_timestamp, label);
	g_menu_remove (G_MENU (win->priv->secondary_menu), 1);
	g_menu_insert_item (G_MENU (win->priv->secondary_menu), 1, win->priv->go_to_timestamp);
	g_free (label);

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "insert");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), result);
}

static void
update_insert_action_sensitivity (GtkClipboard *clip,
                                  GdkEvent     *event,
                                  gpointer      data)
{
	PtWindow  *win = PT_WINDOW (data);

#ifdef GDK_WINDOWING_X11
	GAction    *action;
	GdkDisplay *display;
	display = gtk_clipboard_get_display (clip);
	if (GDK_IS_X11_DISPLAY (display) && !gdk_display_supports_selection_notification (display)) {
		action = g_action_map_lookup_action (G_ACTION_MAP (win), "insert");
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
		return;
	}
#endif

	gtk_clipboard_request_text (
			clip,
			update_insert_action_sensitivity_cb,
			win);
}

static void
update_goto_cursor_action_sensitivity (PtWaveviewer *waveviewer,
                                       gboolean      new,
                                       gpointer      data)
{
	PtWindow  *win = PT_WINDOW (data);
	GAction    *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "goto-cursor");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !new);
}

static void
enable_win_actions (PtWindow *win,
                    gboolean  state)
{
	GAction *action;

#ifdef HAVE_ASR
	/* always active */
	action = g_action_map_lookup_action (G_ACTION_MAP (win), "mode");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
#endif

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "copy");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);

	/* always disable on request, enable only conditionally */
	if (!state) {
		action = g_action_map_lookup_action (G_ACTION_MAP (win), "insert");
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);
	} else {
		update_insert_action_sensitivity (win->priv->clip, NULL, win);
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "goto");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);

	/* always insensitve: either there is no waveform or we are already at cursor position */
	action = g_action_map_lookup_action (G_ACTION_MAP (win), "goto-cursor");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "zoom-in");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "zoom-out");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);
}

static void
pt_window_ready_to_play (PtWindow *win,
                         gboolean  state)
{
	/* Set up widget sensitivity/visibility, actions, labels, window title
	   and timer according to the state of PtPlayer (ready to play or not).
	   Reset tooltips for insensitive widgets. */

	gchar     *display_name = NULL;
	GtkStyleContext *open_context;

	enable_win_actions (win, state);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->priv->button_play), FALSE);

	gtk_widget_set_sensitive (win->priv->button_play, state);
	gtk_widget_set_sensitive (win->priv->button_fast_back, state);
	gtk_widget_set_sensitive (win->priv->button_fast_forward, state);
	gtk_widget_set_sensitive (win->priv->button_jump_back, state);
	gtk_widget_set_sensitive (win->priv->button_jump_forward, state);
	gtk_widget_set_sensitive (win->priv->speed_scale, state);
	gtk_widget_set_sensitive (win->priv->volumebutton, state);

	if (state) {
		open_context = gtk_widget_get_style_context (win->priv->button_open);
		gtk_style_context_remove_class (open_context, "suggested-action");
		display_name = pt_player_get_filename (win->player);
		if (display_name) {
			gtk_window_set_title (GTK_WINDOW (win), display_name);
			g_free (display_name);
		}

		gchar *uri = NULL;
		uri = pt_player_get_uri (win->player);
		if (uri) {
			gtk_recent_manager_add_item (win->priv->recent, uri);
			g_free (uri);
		}

		change_play_button_tooltip (win);
		change_jump_back_tooltip (win);
		change_jump_forward_tooltip (win);
		pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
		add_timer (win);

	} else {
		gtk_label_set_text (GTK_LABEL (win->priv->pos_label), "00:00.0");
		gtk_window_set_title (GTK_WINDOW (win), "Parlatype");
		gtk_widget_set_tooltip_text (win->priv->button_jump_back, NULL);
		gtk_widget_set_tooltip_text (win->priv->button_jump_forward, NULL);
		remove_timer (win);
	}
}

static void
player_error_cb (PtPlayer *player,
                 GError   *error,
                 PtWindow *win)
{
	pt_window_ready_to_play (win, FALSE);
	pt_error_message (win, error->message, NULL);
	/* TODO clear error? */
}

static void
open_cb (PtWaveviewer *viewer,
	 GAsyncResult *res,
	 gpointer     *data)
{
	PtWindow *win = (PtWindow *) data;
	GError	 *error = NULL;

	if (!pt_waveviewer_load_wave_finish (viewer, res, &error)) {
		pt_error_message (win, error->message, NULL);
		g_error_free (error);
		/* TODO that's it? */
		return;
	}
}

gchar*
pt_window_get_uri (PtWindow *win)
{
	return pt_player_get_uri (win->player);
}

void
pt_window_open_file (PtWindow *win,
                     gchar    *uri)
{
	g_return_if_fail (uri != NULL);

	gchar *current_uri;
	gint cmp = 1;

	/* Don't reload already loaded waveform */
	current_uri = pt_player_get_uri (win->player);
	if (current_uri) {
		cmp = g_strcmp0 (current_uri, uri);
		g_free (current_uri);
	}
	if (cmp == 0)
		return;

	pt_window_ready_to_play (win, FALSE);
	if (!pt_player_open_uri (win->player, uri))
		return;
	pt_window_ready_to_play (win, TRUE);
	pt_waveviewer_load_wave_async (PT_WAVEVIEWER (win->priv->waveviewer),
				  uri,
				  NULL,
				  (GAsyncReadyCallback) open_cb,
				  win);
}

static void
update_play_after_toggle (PtWindow        *win,
                          GtkToggleButton *button)
{
	if (gtk_toggle_button_get_active (button)) {
		update_time (win);
		pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
	}

	change_play_button_tooltip (win);
}


static void
player_play_toggled_cb (PtPlayer *player,
                        PtWindow *win)
{
	GtkToggleButton *play;
	play = GTK_TOGGLE_BUTTON (win->priv->button_play);

	/* Player changed play/pause; toggle GUI button and block signals */
	g_signal_handlers_block_by_func (play, play_button_toggled_cb, win);
	gtk_toggle_button_set_active (play, !gtk_toggle_button_get_active (play));
	g_signal_handlers_unblock_by_func (play, play_button_toggled_cb, win);

	update_play_after_toggle (win, play);
}

static void
play_button_toggled_cb (GtkToggleButton *button,
                        PtWindow        *win)
{
	/* GUI button toggled, block signals from PtPlayer */
	g_signal_handlers_block_by_func (win->player, player_play_toggled_cb, win);
	pt_player_play_pause (win->player);
	g_signal_handlers_unblock_by_func (win->player, player_play_toggled_cb, win);

	update_play_after_toggle (win, button);
}

static void
player_end_of_stream_cb (PtPlayer *player,
                         PtWindow *win)
{
	/* Don’t jump back */
	GtkToggleButton *play;
	play = GTK_TOGGLE_BUTTON (win->priv->button_play);

	g_signal_handlers_block_by_func (play, play_button_toggled_cb, win);
	gtk_toggle_button_set_active (play, FALSE);
	g_signal_handlers_unblock_by_func (play, play_button_toggled_cb, win);
	pt_player_pause (win->player);
	change_play_button_tooltip (win);
}

static void
player_jumped_back_cb (PtPlayer *player,
                       PtWindow *win)
{
	GtkButton *button;
	button = GTK_BUTTON (win->priv->button_jump_back);
	g_signal_handlers_block_by_func (button, jump_back_button_clicked_cb, win);
	gtk_button_clicked (button);
	g_signal_handlers_unblock_by_func (button, jump_back_button_clicked_cb, win);
}

static void
jump_back_button_clicked_cb (GtkButton *button,
                             PtWindow  *win)
{
	g_signal_handlers_block_by_func (win->player, player_jumped_back_cb, win);
	pt_player_jump_back (win->player);
	g_signal_handlers_unblock_by_func (win->player, player_jumped_back_cb, win);
}

static void
player_jumped_forward_cb (PtPlayer *player,
                          PtWindow *win)
{
	GtkButton *button;
	button = GTK_BUTTON (win->priv->button_jump_forward);
	g_signal_handlers_block_by_func (button, jump_forward_button_clicked_cb, win);
	gtk_button_clicked (button);
	g_signal_handlers_unblock_by_func (button, jump_forward_button_clicked_cb, win);
}

static void
jump_forward_button_clicked_cb (GtkButton *button,
                                PtWindow  *win)
{
	pt_player_jump_forward (win->player);
}

/* currently not used */
static gboolean
fast_back_button_pressed_cb (GtkButton *button,
                             GdkEvent  *event,
                             PtWindow  *win)
{
	pt_player_rewind (win->player, 2.0);
	return FALSE;
}

/* currently not used */
static gboolean
fast_back_button_released_cb (GtkButton *button,
                              GdkEvent  *event,
                              PtWindow  *win)
{
	pt_player_set_speed (win->player, win->priv->speed);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play))) {
		pt_player_play (win->player);
	} else {
		pt_player_pause (win->player);
	}
	return FALSE;
}

/* currently not used */
static gboolean
fast_forward_button_pressed_cb (GtkButton *button,
                                GdkEvent  *event,
                                PtWindow  *win)
{
	pt_player_fast_forward (win->player, 2.0);
	return FALSE;
}

/* currently not used */
static gboolean
fast_forward_button_released_cb (GtkButton *button,
                                 GdkEvent  *event,
                                 PtWindow  *win)
{
	pt_player_set_speed (win->player, win->priv->speed);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play))) {
		pt_player_play (win->player);
	} else {
		pt_player_pause (win->player);
	}
	return FALSE;
}

#ifdef HAVE_ASR
static void
player_asr_final_cb (PtPlayer *player,
                     gchar    *word,
                     PtWindow *win)
{
	pt_asr_output_final (win->priv->output, word);
}

static void
player_asr_hypothesis_cb (PtPlayer *player,
                          gchar    *word,
                          PtWindow *win)
{
	pt_asr_output_hypothesis (win->priv->output, word);
}
#endif

static void
speed_scale_direction_changed_cb (GtkWidget        *widget,
                                  GtkTextDirection  previous_direction,
                                  gpointer          data)
{
	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		gtk_scale_set_value_pos (GTK_SCALE (widget), GTK_POS_LEFT);
	else
		gtk_scale_set_value_pos (GTK_SCALE (widget), GTK_POS_RIGHT);
}

static void
swap_accelerators (GtkWidget     *widget,
                   GtkAccelGroup *accels,
                   guint          old,
                   guint          new)
{
	gtk_widget_remove_accelerator (
			widget,
			accels,
			old,
			GDK_CONTROL_MASK);
	gtk_widget_add_accelerator (
			widget,
			"clicked",
			accels,
			new,
			GDK_CONTROL_MASK,
			GTK_ACCEL_VISIBLE);
}

static void
jump_back_direction_changed_cb (GtkWidget        *widget,
                                GtkTextDirection  previous_direction,
                                gpointer          data)
{
	PtWindow *self = PT_WINDOW (data);
	guint old, new;

	if (previous_direction == GTK_TEXT_DIR_LTR) {
		old = GDK_KEY_Left;
		new = GDK_KEY_Right;
	} else {
		old = GDK_KEY_Right;
		new = GDK_KEY_Left;
	}

	swap_accelerators (widget, self->priv->accels, old, new);
}

static void
jump_forward_direction_changed_cb (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction,
                                   gpointer          data)
{
	PtWindow *self = PT_WINDOW (data);
	guint old, new;

	if (previous_direction == GTK_TEXT_DIR_LTR) {
		old = GDK_KEY_Right;
		new = GDK_KEY_Left;
	} else {
		old = GDK_KEY_Left;
		new = GDK_KEY_Right;
	}

	swap_accelerators (widget, self->priv->accels, old, new);
}

static void
settings_changed_cb (GSettings *settings,
                     gchar     *key,
                     PtWindow  *win)
{
	if (g_strcmp0 (key, "rewind-on-pause") == 0) {
		change_play_button_tooltip (win);
		return;
	}

	if (g_strcmp0 (key, "jump-back") == 0) {
		change_jump_back_tooltip (win);
		return;
	}

	if (g_strcmp0 (key, "jump-forward") == 0) {
		change_jump_forward_tooltip (win);
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

static GVariant*
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
setup_non_wayland_env (PtWindow *win)
{
	if (g_settings_get_boolean (win->priv->editor, "start-on-top")) {
		gtk_window_set_keep_above (GTK_WINDOW (win), TRUE);
	}
	if (g_settings_get_boolean (win->priv->editor, "remember-position")) {
		gtk_window_move (GTK_WINDOW (win),
				 g_settings_get_int (win->priv->editor, "x-pos"),
				 g_settings_get_int (win->priv->editor, "y-pos"));
	}
}

static void
setup_settings (PtWindow *win)
{
	win->priv->editor = g_settings_new (APP_ID);

	g_settings_bind (
			win->priv->editor, "pps",
			win->priv->waveviewer, "pps",
			G_SETTINGS_BIND_GET);

	g_settings_bind_with_mapping (
			win->priv->editor, "rewind-on-pause",
			win->player, "pause",
			G_SETTINGS_BIND_GET,
			map_seconds_to_milliseconds,
			map_milliseconds_to_seconds,
			NULL, NULL);

	g_settings_bind_with_mapping (
			win->priv->editor, "jump-back",
			win->player, "back",
			G_SETTINGS_BIND_GET,
			map_seconds_to_milliseconds,
			map_milliseconds_to_seconds,
			NULL, NULL);

	g_settings_bind_with_mapping (
			win->priv->editor, "jump-forward",
			win->player, "forward",
			G_SETTINGS_BIND_GET,
			map_seconds_to_milliseconds,
			map_milliseconds_to_seconds,
			NULL, NULL);

	g_settings_bind (
			win->priv->editor, "repeat-all",
			win->player, "repeat-all",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "repeat-selection",
			win->player, "repeat-selection",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "show-ruler",
			win->priv->waveviewer, "show-ruler",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "fixed-cursor",
			win->priv->waveviewer, "fixed-cursor",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "timestamp-precision",
			win->player, "timestamp-precision",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "timestamp-fixed",
			win->player, "timestamp-fixed",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "timestamp-delimiter",
			win->player, "timestamp-delimiter",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "timestamp-fraction-sep",
			win->player, "timestamp-fraction-sep",
			G_SETTINGS_BIND_GET);

	/* connect to tooltip changer */

	g_signal_connect (
			win->priv->editor, "changed",
			G_CALLBACK (settings_changed_cb),
			win);

	/* Default speed
	   Other solutions would be
	   - Automatically save last known speed in GSettings
	   - Add a default speed option to preferences dialog
	   - Save last known speed in metadata for each file */
	win->priv->speed = 1.0;

	if (g_settings_get_boolean (win->priv->editor, "remember-size")) {
		gtk_window_resize (GTK_WINDOW (win),
				   g_settings_get_int (win->priv->editor, "width"),
				   g_settings_get_int (win->priv->editor, "height"));
	}

#ifdef GDK_WINDOWING_X11
	GdkDisplay *display;
	display = gdk_display_get_default ();
	if (GDK_IS_X11_DISPLAY (display))
		setup_non_wayland_env (win);
#endif

#ifdef GDK_WINDOWING_WIN32
	setup_non_wayland_env (win);
#endif
}

static void
volume_button_update_mute (GObject    *gobject,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
	/* Switch icons of volume button depending on mute state.
	 * If muted, only the mute icon is shown (volume can still be adjusted).
	 * In normal state restore original icon set. */

	PtWindow *win = PT_WINDOW (user_data);
	GtkScaleButton *volumebutton = GTK_SCALE_BUTTON (win->priv->volumebutton);
	const gchar* mute_icon [] = {(gchar*)win->priv->vol_icons[0], NULL};

	if (pt_player_get_mute (win->player)) {
		gtk_scale_button_set_icons (volumebutton, mute_icon);
	} else {
		gtk_scale_button_set_icons (volumebutton, (const gchar**) win->priv->vol_icons);
	}
}

static void
volume_button_update_volume (GObject    *gobject,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
	PtWindow *win = PT_WINDOW (user_data);
	GtkScaleButton *volumebutton = GTK_SCALE_BUTTON (win->priv->volumebutton);
	gdouble volume = pt_player_get_volume (win->player);

	gtk_scale_button_set_value (volumebutton, volume);
}

static gboolean
volume_button_value_changed_cb (GtkWidget *volumebutton,
                                gdouble    value,
                                gpointer   user_data)
{
	PtWindow *win = PT_WINDOW (user_data);
	pt_player_set_volume (win->player, value);
	return FALSE;
}

static gboolean
volume_button_event_cb (GtkWidget *volumebutton,
                        GdkEvent  *event,
                        gpointer   user_data)
{
	/* This is for pulseaudiosink in paused state. It doesn't notify of
	 * volume/mute changes. Update values when user is interacting with
	 * the volume button. */

	PtWindow *win = PT_WINDOW (user_data);
	gtk_scale_button_set_value (GTK_SCALE_BUTTON (volumebutton),
	                            pt_player_get_volume (win->player));
	volume_button_update_mute (NULL, NULL, win);
	return FALSE;
}

static gboolean
volume_button_press_event (GtkGestureMultiPress *gesture,
                           gint                  n_press,
                           gdouble               x,
                           gdouble               y,
                           gpointer              user_data)
{
	/* Switch mute state on click with secondary button */

	PtWindow *win = PT_WINDOW (user_data);
	guint     button;
	gboolean  current_mute;

	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

	if (n_press == 1 && button == GDK_BUTTON_SECONDARY) {
		current_mute = pt_player_get_mute (win->player);
		pt_player_set_mute (win->player, !current_mute);
		return TRUE;
	}
	return FALSE;
}

static void
setup_player (PtWindow *win)
{
	GError *error = NULL;

	win->player = pt_player_new ();
	if (!pt_player_setup_player (win->player, &error)) {
		gchar *secondary_message = g_strdup_printf (
			_("Parlatype needs GStreamer 1.x to run. Please check your installation of "
                        "GStreamer and make sure you have the “Good Plugins” installed.\n"
                        "Parlatype will quit now, it received this error message: %s"), error->message);
		pt_error_message (win, _("Fatal error"), secondary_message);
		g_clear_error (&error);
		g_free (secondary_message);
		exit (2);
	}

	pt_player_connect_waveviewer (
			win->player,
			PT_WAVEVIEWER (win->priv->waveviewer));

	g_signal_connect (win->player,
			"end-of-stream",
			G_CALLBACK (player_end_of_stream_cb),
			win);

	g_signal_connect (win->player,
			"error",
			G_CALLBACK (player_error_cb),
			win);

	g_signal_connect (win->player,
			"play-toggled",
			G_CALLBACK (player_play_toggled_cb),
			win);

	g_signal_connect (win->player,
			"jumped-back",
			G_CALLBACK (player_jumped_back_cb),
			win);

	g_signal_connect (win->player,
			"jumped-forward",
			G_CALLBACK (player_jumped_forward_cb),
			win);

	GtkAdjustment *speed_adjustment;
	speed_adjustment = gtk_range_get_adjustment (GTK_RANGE (win->priv->speed_scale));
	g_object_bind_property (
			speed_adjustment, "value",
			win->player, "speed",
			G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

#ifdef HAVE_ASR
	g_signal_connect (win->player,
			"asr-final",
			G_CALLBACK (player_asr_final_cb),
			win);

	g_signal_connect (win->player,
			"asr-hypothesis",
			G_CALLBACK (player_asr_hypothesis_cb),
			win);
#endif
}

static void
setup_volume (PtWindow *win)
{
	/* Connect to volumebutton's "value-changed" signal and to PtPlayer's
	 * "notify:volume" signal instead of binding the 2 properties. On
	 * startup there is no external Pulseaudio volume yet and the binding
	 * would impose our default volume on Pulseaudio. Instead we want to
	 * pick up Pulseaudio's volume. That is Parlatype/application specific
	 * and it even remembers the last value. The first external volume
	 * signal is sent after a file was opened successfully, until then our
	 * volume button is insensitive. */
	g_signal_connect (win->priv->volumebutton,
			"value-changed",
			G_CALLBACK (volume_button_value_changed_cb),
			win);

	g_signal_connect (win->priv->volumebutton,
			"event",
			G_CALLBACK (volume_button_event_cb),
			win);

	/* Get complete icon set of volume button to change it depending on
	 * mute state. */
	g_object_get (win->priv->volumebutton,
	              "icons", &win->priv->vol_icons, NULL);

	g_signal_connect (win->player,
			"notify::mute",
			G_CALLBACK (volume_button_update_mute),
			win);

	g_signal_connect (win->player,
			"notify::volume",
			G_CALLBACK (volume_button_update_volume),
			win);

	/* Switch mute state on mouse click with secondary button */
	win->priv->vol_event = gtk_gesture_multi_press_new (
			win->priv->volumebutton);
	gtk_gesture_single_set_exclusive (
			GTK_GESTURE_SINGLE (win->priv->vol_event), TRUE);
	gtk_gesture_single_set_button (
			GTK_GESTURE_SINGLE (win->priv->vol_event), 0);
	gtk_event_controller_set_propagation_phase (
			GTK_EVENT_CONTROLLER (win->priv->vol_event), GTK_PHASE_CAPTURE);
	g_signal_connect (win->priv->vol_event,
			"pressed",
			G_CALLBACK (volume_button_press_event),
			win);
}

static void
setup_accels_actions_headerbar (PtWindow *win)
{
	/* Actions */	
	g_action_map_add_action_entries (G_ACTION_MAP (win),
				win_actions,
				G_N_ELEMENTS (win_actions),
				win);

	enable_win_actions (win, FALSE);

	/* GtkHeader workaround for glade 3.16 + Menu button */
	GtkBuilder    *builder;
	GtkWidget     *hbar;
	GMenuModel    *primary_menu;

	builder = gtk_builder_new_from_resource ("/org/parlatype/parlatype/window-headerbar.ui");
	hbar = GTK_WIDGET (gtk_builder_get_object (builder, "headerbar"));
	win->priv->button_open = GTK_WIDGET (gtk_builder_get_object (builder, "button_open"));
	win->priv->primary_menu_button = GTK_WIDGET (gtk_builder_get_object (builder, "primary_menu_button"));
	gtk_window_set_titlebar (GTK_WINDOW (win), hbar);
	gtk_builder_connect_signals (builder, win);
	g_object_unref (builder);

	builder = gtk_builder_new_from_resource ("/org/parlatype/parlatype/menus.ui");
	primary_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "primary-menu"));
	win->priv->secondary_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "secondary-menu"));
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (win->priv->primary_menu_button), primary_menu);
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (win->priv->pos_menu_button), win->priv->secondary_menu);
	g_object_unref (builder);
	win->priv->go_to_timestamp = g_menu_item_new (_("Go to time in clipboard"), "win.insert");
	g_menu_insert_item (G_MENU (win->priv->secondary_menu), 1, win->priv->go_to_timestamp);

	GApplication *app;
	app = g_application_get_default ();
	if (!pt_app_get_asr (PT_APP (app))) {
		g_menu_remove (G_MENU (primary_menu), 0);
	}

	/* Accels */
	win->priv->accels = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (win), win->priv->accels);
	
	gtk_widget_add_accelerator (
			win->priv->primary_menu_button,
			"clicked",
			win->priv->accels,
			GDK_KEY_F10,
			0,
			GTK_ACCEL_VISIBLE);

	guint jump_back, jump_forward;
	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_LTR) {
		jump_back = GDK_KEY_Left;
		jump_forward = GDK_KEY_Right;
	} else {
		jump_back = GDK_KEY_Right;
		jump_forward = GDK_KEY_Left;
	}

	gtk_widget_add_accelerator (
			win->priv->button_jump_back,
			"clicked",
			win->priv->accels,
			jump_back,
			GDK_CONTROL_MASK,
			GTK_ACCEL_VISIBLE);

	gtk_widget_add_accelerator (
			win->priv->button_jump_forward,
			"clicked",
			win->priv->accels,
			jump_forward,
			GDK_CONTROL_MASK,
			GTK_ACCEL_VISIBLE);

	gtk_widget_add_accelerator (
			win->priv->button_play,
			"clicked",
			win->priv->accels,
			GDK_KEY_space,
			GDK_CONTROL_MASK,
			GTK_ACCEL_VISIBLE);
}

static void
pt_window_init (PtWindow *win)
{
	win->priv = pt_window_get_instance_private (win);

	gtk_widget_init_template (GTK_WIDGET (win));

	setup_player (win);
	setup_settings (win);
	setup_accels_actions_headerbar (win);
	setup_volume (win);
	pt_window_setup_dnd (win);	/* this is in pt_window_dnd.c */
#ifdef HAVE_ASR
	win->priv->asr_settings = NULL;
	win->priv->output = NULL;
	win->priv->output_handler_id1 = 0;
	win->priv->output_handler_id2 = 0;
#endif
	win->priv->recent = gtk_recent_manager_get_default ();
	win->priv->timer = 0;
	win->priv->last_time = 0;
	win->priv->clip_handler_id = 0;
	win->priv->clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	/* Used e.g. by Xfce */
	gtk_window_set_default_icon_name (APP_ID);

	/* Flip speed scale for right to left layouts */
	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
		gtk_scale_set_value_pos (GTK_SCALE (win->priv->speed_scale), GTK_POS_LEFT);

	pt_window_ready_to_play (win, FALSE);

	win->priv->clip_handler_id = g_signal_connect (win->priv->clip,
			"owner-change",
			G_CALLBACK (update_insert_action_sensitivity),
			win);
	g_signal_connect (win->priv->waveviewer,
			"follow-cursor-changed",
			G_CALLBACK (update_goto_cursor_action_sensitivity),
			win);

	g_signal_connect_swapped (win->priv->waveviewer,
				  "load-progress",
				  G_CALLBACK (gtk_progress_bar_set_fraction),
				  GTK_PROGRESS_BAR (win->priv->progress));
}

static void
pt_window_dispose (GObject *object)
{
	PtWindow *win;
	win = PT_WINDOW (object);

	/* Save window size/position on all backends */
	if (win->priv->editor) {
		gint x;
		gint y;
		gtk_window_get_position (GTK_WINDOW (win), &x, &y);
		g_settings_set_int (win->priv->editor, "x-pos", x);
		g_settings_set_int (win->priv->editor, "y-pos", y);
		gtk_window_get_size (GTK_WINDOW (win), &x, &y);
		g_settings_set_int (win->priv->editor, "width", x);
		g_settings_set_int (win->priv->editor, "height", y);
	}

	if (win->priv->clip_handler_id > 0) {
		g_signal_handler_disconnect (win->priv->clip, win->priv->clip_handler_id);
		win->priv->clip_handler_id = 0;
	}
	remove_timer (win);
	g_clear_object (&win->priv->editor);
	g_clear_object (&win->player);
#ifdef HAVE_ASR
	g_clear_object (&win->priv->output);
	g_clear_object (&win->priv->asr_settings);
#endif
	g_clear_object (&win->priv->go_to_timestamp);
	g_clear_object (&win->priv->vol_event);

	G_OBJECT_CLASS (pt_window_parent_class)->dispose (object);
}

static void
pt_window_class_init (PtWindowClass *klass)
{
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	gobject_class->dispose      = pt_window_dispose;
	gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/window.ui");
	gtk_widget_class_bind_template_callback(widget_class, fast_back_button_pressed_cb);
	gtk_widget_class_bind_template_callback(widget_class, fast_back_button_released_cb);
	gtk_widget_class_bind_template_callback(widget_class, fast_forward_button_pressed_cb);
	gtk_widget_class_bind_template_callback(widget_class, fast_forward_button_released_cb);
	gtk_widget_class_bind_template_callback(widget_class, jump_back_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, jump_back_direction_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, jump_forward_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, jump_forward_direction_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, play_button_toggled_cb);
	gtk_widget_class_bind_template_callback(widget_class, speed_scale_direction_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, zoom_in_cb);
	gtk_widget_class_bind_template_callback(widget_class, zoom_out_cb);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, progress);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, button_play);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, button_fast_back);	// not used
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, button_fast_forward);	// not used
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, button_jump_back);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, button_jump_forward);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, volumebutton);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, pos_menu_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, pos_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, speed_scale);
	gtk_widget_class_bind_template_child_private (widget_class, PtWindow, waveviewer);
}

PtWindow *
pt_window_new (PtApp *app)
{
	return g_object_new (PT_WINDOW_TYPE, "application", app, NULL);
}
