/* Copyright (C) Gabor Karsay 2016 <gabor.karsay@gmx.at>
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <pt-player.h>
#include <pt-progress-dialog.h>
#include <pt-waveviewer.h>
#include "pt-app.h"
#include "pt-dbus-service.h"
#include "pt-goto-dialog.h"
#include "pt-mediakeys.h"
#include "pt-window.h"
#include "pt-window-dnd.h"
#include "pt-window-private.h"


enum
{
	PROP_0,
	PROP_BACK,
	PROP_FORWARD,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (PtWindow, pt_window, GTK_TYPE_APPLICATION_WINDOW)


void
pt_error_message (PtWindow    *parent,
		  const gchar *message)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (
			GTK_WINDOW (parent),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s", message);

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

	timestamp = pt_player_get_timestamp (win->priv->player);
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
		pt_player_goto_timestamp (win->priv->player, timestamp);
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
	pt_goto_dialog_set_pos (dlg, pt_player_get_position (win->priv->player) / 1000);
	pt_goto_dialog_set_max (dlg, pt_player_get_duration (win->priv->player) / 1000);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK) {
		pos = pt_goto_dialog_get_pos (dlg);
		pt_player_jump_to_position (win->priv->player, pos * 1000);
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

const GActionEntry win_actions[] = {
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

	text = pt_player_get_current_time_string (win->priv->player, PT_PRECISION_SECOND_10TH);

	if (text == NULL)
		return;

	gtk_label_set_text (GTK_LABEL (win->priv->pos_label), text);
	g_free (text);

	g_object_set (win->priv->waveviewer,
		      "playback-cursor",
		      pt_player_get_position (win->priv->player),
		      NULL);
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

void
cursor_changed_cb (GtkWidget *widget,
		   gint64     pos,
		   PtWindow  *win)
{
	/* triggered only by user */

	pt_player_jump_to_position (win->priv->player, pos);
	update_time (win);
	pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
}

void
selection_changed_cb (GtkWidget *widget,
		      PtWindow  *win)
{
	gint64 start, end, current;
	if (win->priv->playing_selection) {

		current = pt_player_get_position (win->priv->player);
		g_object_get (win->priv->waveviewer,
			      "selection-start", &start,
			      "selection-end", &end,
			      NULL);
		if (start <= current && current <= end) {
			pt_player_set_selection (win->priv->player, start, end);
		} else {
			pt_player_clear_selection (win->priv->player);
			win->priv->playing_selection = FALSE;
		}
	}
}

void
play_toggled_cb (GtkWidget *widget,
		 PtWindow  *win)
{
	/* Callback from waveviewer */
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (win->priv->button_play),
		!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play)));
}

void
zoom_in_cb (GtkWidget *widget,
	    PtWindow  *win)
{
	GAction  *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "zoom-in");
	g_action_activate (action, NULL);
}

void
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

	back = g_strdup_printf (
			ngettext ("Jump back 1 second",
				  "Jump back %d seconds",
				  win->priv->back),
			win->priv->back);

	gtk_widget_set_tooltip_text (win->priv->button_jump_back, back);
	g_free (back);
}

static void
change_jump_forward_tooltip (PtWindow *win)
{
	gchar *forward;

	forward = g_strdup_printf (
			ngettext ("Jump forward 1 second",
				  "Jump forward %d seconds",
				  win->priv->forward),
			win->priv->forward);

	gtk_widget_set_tooltip_text (win->priv->button_jump_forward, forward);
	g_free (forward);
}

static void
change_play_button_tooltip (PtWindow *win)
{
	gchar    *play;
	gint      pause;
	gboolean  free_me = FALSE;

	pause = pt_player_get_pause (win->priv->player) / 1000;
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
	PtPlayer *player = win->priv->player;
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
	GAction      *action;

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
destroy_progress_dlg (PtWindow *win)
{
	if (win->priv->progress_handler_id > 0) {
		g_signal_handler_disconnect (win->priv->player, win->priv->progress_handler_id);
		win->priv->progress_handler_id = 0;
	}

	if (win->priv->progress_dlg) {
		gtk_widget_destroy (win->priv->progress_dlg);
	}
}

static void
progress_response_cb (GtkWidget *dialog,
		      gint       response,
		      PtWindow  *win)
{
	if (response == GTK_RESPONSE_CANCEL)
		pt_player_cancel (win->priv->player);
		/* This will trigger an error */
}

static void
show_progress_dlg (PtWindow *win)
{
	if (win->priv->progress_dlg) {
		/* actually this should never happen */
		gtk_window_present (GTK_WINDOW (win->priv->progress_dlg));
		return;
	}

	win->priv->progress_dlg = GTK_WIDGET (pt_progress_dialog_new (GTK_WINDOW (win)));

	g_signal_connect (win->priv->progress_dlg,
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &win->priv->progress_dlg);

	g_signal_connect (win->priv->progress_dlg,
			  "response",
			  G_CALLBACK (progress_response_cb),
			  win);

	win->priv->progress_handler_id =
		g_signal_connect_swapped (win->priv->player,
					  "load-progress",
					  G_CALLBACK (pt_progress_dialog_set_progress),
					  PT_PROGRESS_DIALOG (win->priv->progress_dlg));

	gtk_widget_show_all (win->priv->progress_dlg);
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

	if (state) {
		open_context = gtk_widget_get_style_context (win->priv->button_open);
		gtk_style_context_remove_class (open_context, "suggested-action");
		destroy_progress_dlg (win);
		display_name = pt_player_get_filename (win->priv->player);
		if (display_name) {
			gtk_window_set_title (GTK_WINDOW (win), display_name);
			g_free (display_name);
		}

		gchar *uri = NULL;
		uri = pt_player_get_uri (win->priv->player);
		if (uri) {
			gtk_recent_manager_add_item (win->priv->recent, uri);
			g_free (uri);
		}

		change_play_button_tooltip (win);
		change_jump_back_tooltip (win);
		change_jump_forward_tooltip (win);
		pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
		win->priv->wavedata = pt_player_get_data (win->priv->player,
							  g_settings_get_int (win->priv->editor, "pps"));
		pt_waveviewer_set_wave (PT_WAVEVIEWER (win->priv->waveviewer),
					win->priv->wavedata);
		pt_wavedata_free (win->priv->wavedata);
		/* add timer after waveviewer, didn't update cursor otherwise sometimes */
		add_timer (win);

	} else {
		gtk_label_set_text (GTK_LABEL (win->priv->pos_label), "00:00.0");
		gtk_window_set_title (GTK_WINDOW (win), "Parlatype");
		gtk_widget_set_tooltip_text (win->priv->button_jump_back, NULL);
		gtk_widget_set_tooltip_text (win->priv->button_jump_forward, NULL);
		remove_timer (win);
		pt_waveviewer_set_wave (PT_WAVEVIEWER (win->priv->waveviewer), NULL);
	}
}

static void
player_error_cb (PtPlayer *player,
		 GError   *error,
		 PtWindow *win)
{
	destroy_progress_dlg (win);
	pt_window_ready_to_play (win, FALSE);
	pt_error_message (win, error->message);
}

static void
open_cb (PtPlayer     *player,
	 GAsyncResult *res,
	 gpointer     *data)
{
	PtWindow *win = (PtWindow *) data;
	GError	 *error = NULL;

	destroy_progress_dlg (win);

	if (!pt_player_open_uri_finish (player, res, &error)) {
		pt_error_message (win, error->message);
		g_error_free (error);
		return;
	}

	pt_window_ready_to_play (win, TRUE);
}

gchar*
pt_window_get_uri (PtWindow *win)
{
	return pt_player_get_uri (win->priv->player);
}

void
pt_window_open_file (PtWindow *win,
		     gchar    *uri)
{
	show_progress_dlg (win);
	pt_window_ready_to_play (win, FALSE);
	pt_player_open_uri_async (win->priv->player,
				  uri,
				  (GAsyncReadyCallback) open_cb,
				  win);
}

void
play_button_toggled_cb (GtkToggleButton *button,
			PtWindow	*win)
{
	gint64 start, end;
	gboolean selection;

	if (gtk_toggle_button_get_active (button)) {

		/* If there is a selection, play it */
		g_object_get (win->priv->waveviewer,
			      "selection-start", &start,
			      "selection-end", &end,
			      "has-selection", &selection,
			      NULL);

		if (!win->priv->playing_selection && selection) {
			/* Note: changes position if outside selection */
			pt_player_set_selection (win->priv->player, start, end);
			win->priv->playing_selection = TRUE;
		}

		pt_player_play (win->priv->player);
		update_time (win);
		pt_waveviewer_set_follow_cursor (PT_WAVEVIEWER (win->priv->waveviewer), TRUE);
	} else {
		pt_player_pause_and_rewind (win->priv->player);
	}

	change_play_button_tooltip (win);
}

static void
player_end_of_stream_cb (PtPlayer *player,
			 PtWindow *win)
{
	/* Don't jump back */
	GtkToggleButton *play;
	play = GTK_TOGGLE_BUTTON (win->priv->button_play);

	g_signal_handlers_block_by_func (play, play_button_toggled_cb, win);
	gtk_toggle_button_set_active (play, FALSE);
	g_signal_handlers_unblock_by_func (play, play_button_toggled_cb, win);
	pt_player_pause (win->priv->player);
	change_play_button_tooltip (win);
}

void
jump_back_button_clicked_cb (GtkButton *button,
			     PtWindow  *win)
{
	pt_player_jump_relative (win->priv->player, win->priv->back * -1000);
}

void
jump_forward_button_clicked_cb (GtkButton *button,
			        PtWindow  *win)
{
	pt_player_jump_relative (win->priv->player, win->priv->forward * 1000);
}

/* currently not used */
gboolean
fast_back_button_pressed_cb (GtkButton *button,
			     GdkEvent  *event,
			     PtWindow  *win)
{
	pt_player_rewind (win->priv->player, 2.0);
	return FALSE;
}

/* currently not used */
gboolean
fast_back_button_released_cb (GtkButton *button,
			      GdkEvent  *event,
			      PtWindow  *win)
{
	pt_player_set_speed (win->priv->player, win->priv->speed);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play))) {
		pt_player_play (win->priv->player);
	} else {
		pt_player_pause (win->priv->player);
	}
	return FALSE;
}

/* currently not used */
gboolean
fast_forward_button_pressed_cb (GtkButton *button,
			        GdkEvent  *event,
				PtWindow  *win)
{
	pt_player_fast_forward (win->priv->player, 2.0);
	return FALSE;
}

/* currently not used */
gboolean
fast_forward_button_released_cb (GtkButton *button,
			         GdkEvent  *event,
				 PtWindow  *win)
{
	pt_player_set_speed (win->priv->player, win->priv->speed);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play))) {
		pt_player_play (win->priv->player);
	} else {
		pt_player_pause (win->priv->player);
	}
	return FALSE;
}

void
speed_changed_cb (GtkRange *range,
		  PtWindow *win)
{
	win->priv->speed = gtk_range_get_value (range);
	pt_player_set_speed (win->priv->player, win->priv->speed);
}

void
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

void
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

void
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

	if (g_strcmp0 (key, "pps") == 0) {
		gchar *uri;
		uri = pt_player_get_uri (win->priv->player);
		if (uri) {
			win->priv->wavedata = pt_player_get_data (win->priv->player,
								  g_settings_get_int (win->priv->editor, "pps"));
			g_object_set (win->priv->waveviewer, "zoom", TRUE, NULL);
			pt_waveviewer_set_wave (PT_WAVEVIEWER (win->priv->waveviewer),
						win->priv->wavedata);
			pt_wavedata_free (win->priv->wavedata);
			g_free (uri);
		}
		return;
	}
}

static gboolean
get_pause_mapping (GValue   *value,
	           GVariant *variant,
	           gpointer  data)
{
	/* Settings store seconds, PtPlayer wants milliseconds */
	gint pause;
	pause = g_variant_get_int32 (variant);
	pause = pause * 1000;
	g_value_set_int (value, pause);
	return TRUE;
}

static GVariant*
set_pause_mapping (const GValue       *value,
	           const GVariantType *type,
	           gpointer            data)
{
	gint pause;
	pause = g_value_get_int (value);
	pause = pause / 1000;
	return g_variant_new_int32 (pause);
}

static void
setup_settings (PtWindow *win)
{
	win->priv->editor = g_settings_new ("com.github.gkarsay.parlatype");

	g_settings_bind_with_mapping (
			win->priv->editor, "rewind-on-pause",
			win->priv->player, "pause",
			G_SETTINGS_BIND_GET,
			get_pause_mapping, set_pause_mapping,
			NULL, NULL);

	g_settings_bind (
			win->priv->editor, "jump-back",
			win, "back",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "jump-forward",
			win, "forward",
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
			win->priv->player, "timestamp-precision",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "timestamp-fixed",
			win->priv->player, "timestamp-fixed",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "timestamp-delimiter",
			win->priv->player, "timestamp-delimiter",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "timestamp-fraction-sep",
			win->priv->player, "timestamp-fraction-sep",
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

	/* Set size on both X11 and Wayland */
	if (g_settings_get_boolean (win->priv->editor, "remember-size")) {
		gtk_window_resize (GTK_WINDOW (win),
				   g_settings_get_int (win->priv->editor, "width"),
				   g_settings_get_int (win->priv->editor, "height"));
	}

	/* The following would fail silently on Wayland, we make it conditional anyway */
#ifdef GDK_WINDOWING_X11
	GdkDisplay *display;
	display = gdk_display_get_default ();
	if (GDK_IS_X11_DISPLAY (display)) {
		if (g_settings_get_boolean (win->priv->editor, "start-on-top")) {
			gtk_window_set_keep_above (GTK_WINDOW (win), TRUE);
		}
		if (g_settings_get_boolean (win->priv->editor, "remember-position")) {
			gtk_window_move (GTK_WINDOW (win),
					 g_settings_get_int (win->priv->editor, "x-pos"),
					 g_settings_get_int (win->priv->editor, "y-pos"));
		}
	}
#endif
}

static void
setup_player (PtWindow *win)
{
	/* Already tested in main.c, we don't check for errors here anymore */
	win->priv->player = pt_player_new (NULL);

	g_signal_connect (win->priv->player,
			"end-of-stream",
			G_CALLBACK (player_end_of_stream_cb),
			win);

	g_signal_connect (win->priv->player,
			"error",
			G_CALLBACK (player_error_cb),
			win);

	g_object_bind_property (
			win->priv->volumebutton, "value",
			win->priv->player, "volume",
			G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
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
	GtkWidget     *primary_menu_button;
	GMenuModel    *primary_menu;

	builder = gtk_builder_new_from_resource ("/com/github/gkarsay/parlatype/window-headerbar.ui");
	hbar = GTK_WIDGET (gtk_builder_get_object (builder, "headerbar"));
	win->priv->button_open = GTK_WIDGET (gtk_builder_get_object (builder, "button_open"));
	primary_menu_button = GTK_WIDGET (gtk_builder_get_object (builder, "primary_menu_button"));
	gtk_window_set_titlebar (GTK_WINDOW (win), hbar);
	gtk_builder_connect_signals (builder, win);
	g_object_unref (builder);

	builder = gtk_builder_new_from_resource ("/com/github/gkarsay/parlatype/menus.ui");
	primary_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "primary-menu"));
	win->priv->secondary_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "secondary-menu"));
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (primary_menu_button), primary_menu);
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (win->priv->pos_menu_button), win->priv->secondary_menu);
	g_object_unref (builder);
	win->priv->go_to_timestamp = g_menu_item_new (_("Go to time in clipboard"), "win.insert");
	g_menu_insert_item (G_MENU (win->priv->secondary_menu), 1, win->priv->go_to_timestamp);

	/* Accels */
	win->priv->accels = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (win), win->priv->accels);
	
	gtk_widget_add_accelerator (
			primary_menu_button,
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
	setup_mediakeys (win);		/* this is in pt_mediakeys.c */
	pt_window_setup_dnd (win);	/* this is in pt_window_dnd.c */
	setup_dbus_service (win);	/* this is in pt_dbus_service.c */
	win->priv->recent = gtk_recent_manager_get_default ();
	win->priv->timer = 0;
	win->priv->progress_dlg = NULL;
	win->priv->progress_handler_id = 0;
	win->priv->wavedata = NULL;
	win->priv->playing_selection = FALSE;
	win->priv->clip_handler_id = 0;
	win->priv->clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	/* Used e.g. by Xfce */
	gtk_window_set_default_icon_name ("com.github.gkarsay.parlatype");

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
}

static void
pt_window_dispose (GObject *object)
{
	PtWindow *win;
	win = PT_WINDOW (object);

	close_dbus_service (win);

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
	g_clear_object (&win->priv->proxy);
	destroy_progress_dlg (win);
	g_clear_object (&win->priv->player);

	G_OBJECT_CLASS (pt_window_parent_class)->dispose (object);
}

static void
pt_window_set_property (GObject      *object,
			guint         property_id,
			const GValue *value,
			GParamSpec   *pspec)
{
	PtWindow *win;
	win = PT_WINDOW (object);

	switch (property_id) {
	case PROP_BACK:
		win->priv->back = g_value_get_int (value);
		break;
	case PROP_FORWARD:
		win->priv->forward = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_window_get_property (GObject    *object,
			guint       property_id,
			GValue     *value,
			GParamSpec *pspec)
{
	PtWindow *win;
	win = PT_WINDOW (object);

	switch (property_id) {
	case PROP_BACK:
		g_value_set_int (value, win->priv->back);
		break;
	case PROP_FORWARD:
		g_value_set_int (value, win->priv->forward);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_window_class_init (PtWindowClass *klass)
{
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = pt_window_set_property;
	gobject_class->get_property = pt_window_get_property;
	gobject_class->dispose      = pt_window_dispose;
	gtk_widget_class_set_template_from_resource (widget_class, "/com/github/gkarsay/parlatype/window.ui");
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

	obj_properties[PROP_BACK] =
	g_param_spec_int (
			"back",
			"Seconds to jump back",
			"Seconds to jump back",
			1,	/* minimum */
			60,	/* maximum */
			10,
			G_PARAM_READWRITE);

	obj_properties[PROP_FORWARD] =
	g_param_spec_int (
			"forward",
			"Seconds to jump forward",
			"Seconds to jump forward",
			1,	/* minimum */
			60,	/* maximum */
			10,
			G_PARAM_READWRITE);

	g_object_class_install_properties (
			gobject_class,
			N_PROPERTIES,
			obj_properties);
}

PtWindow *
pt_window_new (PtApp *app)
{
	return g_object_new (PT_WINDOW_TYPE, "application", app, NULL);
}
