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
#include "pt-app.h"
#include "pt-player.h"
#include "pt-mediakeys.h"
#include "pt-window.h"
#include "pt-window-dnd.h"
#include "pt-window-private.h"


enum
{
	PROP_0,
	PROP_PAUSE,
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

	GtkClipboard *clip;
	const gchar  *timestamp = NULL;

	timestamp = pt_player_get_timestamp (win->priv->player);

	if (timestamp) {
		clip = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
		gtk_clipboard_set_text (clip, timestamp, -1);
	}
}

const GActionEntry win_actions[] = {
	{ "copy", copy_timestamp, NULL, NULL, NULL }
};

void
time_scale_changed_cb (GtkRange *range,
		       PtWindow *win)
{
	/* triggered only by user */

	GtkAdjustment *adj;
	gdouble        pos, new;

	adj = gtk_range_get_adjustment (range);
	pos = gtk_adjustment_get_value (adj);
	
	pt_player_jump_to_permille (win->priv->player, pos);
}

static void
update_duration_label (PtWindow *win)
{
	gchar *text;
	text = pt_player_get_duration_time_string (win->priv->player, 0);
	if (text) {
		gtk_label_set_text (GTK_LABEL (win->priv->dur_label), text);
		g_debug ("duration: %s", text);
		g_free (text);
	}
}

static gboolean
update_time (PtWindow *win)
{
	gint   permille;
	gchar *text;

	text = pt_player_get_current_time_string (win->priv->player, 0);
	permille = pt_player_get_permille (win->priv->player);

	if (permille == -1 || text == NULL)
		return TRUE;

	gtk_label_set_text (GTK_LABEL (win->priv->pos_label), text);
	g_free (text);

	/* update time_scale but block any signals */
	g_signal_handlers_block_by_func (win->priv->time_scale, time_scale_changed_cb, win);
	gtk_adjustment_set_value (GTK_ADJUSTMENT (win->priv->time_adj), permille);
	g_signal_handlers_unblock_by_func (win->priv->time_scale, time_scale_changed_cb, win);

	return TRUE;
}

static void
add_timer (PtWindow *win)
{
	if (win->priv->timer == 0) {
		win->priv->timer = g_timeout_add (10, (GSourceFunc) update_time, win);
	}
}

static void
remove_timer (PtWindow *win)
{
	if (win->priv->timer > 0) {
		g_source_remove (win->priv->timer);
		win->priv->timer = 0;
	}
}

static void
player_state_changed_cb (PtPlayer *player,
			 gboolean  state,
			 PtWindow *win)
{
	/* Set up widget sensitivity/visibility, actions, labels, window title
	   and timer according to the state of PtPlayer (ready to play or not) */

	GAction   *action;
	gchar     *display_name = NULL;
	GdkWindow *gdkwin;

	action = g_action_map_lookup_action (G_ACTION_MAP (win), "copy");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), state);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->priv->button_play), FALSE);
	gtk_widget_set_visible (win->priv->button_play, state);
	gtk_widget_set_visible (win->priv->button_open, !state);

	gtk_widget_set_sensitive (win->priv->button_play, state);
	gtk_widget_set_sensitive (win->priv->button_fast_back, state);
	gtk_widget_set_sensitive (win->priv->button_fast_forward, state);
	gtk_widget_set_sensitive (win->priv->button_jump_back, state);
	gtk_widget_set_sensitive (win->priv->button_jump_forward, state);
	gtk_widget_set_sensitive (win->priv->time_scale, state);
	gtk_widget_set_sensitive (win->priv->speed_scale, state);

	if (state) {
		update_duration_label (win);
		display_name = pt_player_get_filename (player);
		if (display_name) {
			gtk_window_set_title (GTK_WINDOW (win), display_name);
			g_free (display_name);
		}
		gtk_recent_manager_add_item (
				win->priv->recent,
				pt_player_get_uri (player));
		add_timer (win);

		gdkwin = gtk_widget_get_window (GTK_WIDGET (win));
		gdk_window_set_cursor (gdkwin, NULL);

	} else {
		gtk_label_set_text (GTK_LABEL (win->priv->dur_label), "00:00");
		gtk_label_set_text (GTK_LABEL (win->priv->pos_label), "00:00");
		gtk_window_set_title (GTK_WINDOW (win), "Parlatype");
		remove_timer (win);
	}
}

static void
player_duration_changed_cb (PtPlayer *player,
			    PtWindow *win)
{
	g_debug ("duration_changed_cb");
	update_duration_label (win);
}

static void
player_end_of_stream_cb (PtPlayer *player,
			 PtWindow *win)
{
	g_debug ("end_of_stream_cb");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->priv->button_play), FALSE);
}

static void
player_error_cb (PtPlayer *player,
		 GError   *error,
		 PtWindow *win)
{
	GdkWindow  *gdkwin;

	gdkwin = gtk_widget_get_window (GTK_WIDGET (win));
	gdk_window_set_cursor (gdkwin, NULL);

	pt_error_message (win, error->message);
}

void
pt_window_open_file (PtWindow *win,
		     gchar    *uri)
{
	GdkWindow  *gdkwin;
	GdkDisplay *display;
	GdkCursor  *cur;

	gdkwin = gtk_widget_get_window (GTK_WIDGET (win));
	display = gdk_window_get_display (gdkwin);
	cur = gdk_cursor_new_from_name (display, "watch");
	gdk_window_set_cursor (gdkwin, cur);
	if (cur)
		g_object_unref (cur);

	pt_player_open_uri (win->priv->player, uri);
}

void
play_button_toggled_cb (GtkToggleButton *button,
			PtWindow	*win)
{
	if (gtk_toggle_button_get_active (button)) {
		pt_player_play (win->priv->player);
	} else {
		pt_player_pause (win->priv->player);
		pt_player_jump_relative (win->priv->player, win->priv->pause * -1000);
	}
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
	g_debug ("rewind");
	pt_player_rewind (win->priv->player, 2.0);
	return FALSE;
}

/* currently not used */
gboolean
fast_back_button_released_cb (GtkButton *button,
			      GdkEvent  *event,
			      PtWindow  *win)
{
	g_debug ("rewind stop");
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
	g_debug ("fast forward");
	pt_player_fast_forward (win->priv->player, 2.0);
	return FALSE;
}

/* currently not used */
gboolean
fast_forward_button_released_cb (GtkButton *button,
			         GdkEvent  *event,
				 PtWindow  *win)
{
	g_debug ("fast forward stop");
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

static void
setup_settings (PtWindow *win)
{
	win->priv->editor = g_settings_new ("org.gnome.parlatype");

	g_settings_bind (
			win->priv->editor, "rewind-on-pause",
			win, "pause",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "jump-back",
			win, "back",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			win->priv->editor, "jump-forward",
			win, "forward",
			G_SETTINGS_BIND_GET);

	/* Default speed
	   Other solutions would be
	   - Automatically save last known speed in GSettings
	   - Add a default speed option to preferences dialog
	   - Save last known speed in metadata for each file */
	win->priv->speed = 1.0;

	if (g_settings_get_boolean (win->priv->editor, "start-on-top")) {
		gtk_window_set_keep_above (GTK_WINDOW (win), TRUE);
	}

	if (g_settings_get_boolean (win->priv->editor, "remember-position")) {
		gtk_window_move (GTK_WINDOW (win),
				 g_settings_get_int (win->priv->editor, "x-pos"),
				 g_settings_get_int (win->priv->editor, "y-pos"));
		gtk_window_resize (GTK_WINDOW (win),
				   g_settings_get_int (win->priv->editor, "width"),
				   g_settings_get_int (win->priv->editor, "height"));
	}
}

static void
setup_player (PtWindow *win)
{
	/* Already tested in main.c, we don't check for errors here anymore */
	win->priv->player = pt_player_new (win->priv->speed, NULL);

	g_signal_connect (win->priv->player,
			"player-state-changed",
			G_CALLBACK (player_state_changed_cb),
			win);

	g_signal_connect (win->priv->player,
			"duration-changed",
			G_CALLBACK (player_duration_changed_cb),
			win);

	g_signal_connect (win->priv->player,
			"end-of-stream",
			G_CALLBACK (player_end_of_stream_cb),
			win);

	g_signal_connect (win->priv->player,
			"error",
			G_CALLBACK (player_error_cb),
			win);

	g_object_bind_property (win->priv->volumebutton, "value",
			win->priv->player, "volume",G_BINDING_DEFAULT);
}

static void
setup_accels_actions_headerbar (PtWindow *win)
{
	/* Actions */	
	g_action_map_add_action_entries (G_ACTION_MAP (win),
				win_actions,
				G_N_ELEMENTS (win_actions),
				win);

	/* GtkHeader workaround for glade 3.16 + Menu button */
	GtkBuilder    *builder;
	GMenuModel    *model;
	GtkWidget     *hbar;
	GtkWidget     *add_button;
	GtkWidget     *menu_button;

	builder = gtk_builder_new_from_resource ("/org/gnome/parlatype/window-headerbar.ui");
	hbar = GTK_WIDGET (gtk_builder_get_object (builder, "headerbar"));
	win->priv->button_open = GTK_WIDGET (gtk_builder_get_object (builder, "button_open"));
	win->priv->button_play = GTK_WIDGET (gtk_builder_get_object (builder, "button_play"));
	menu_button = GTK_WIDGET (gtk_builder_get_object (builder, "menu_button"));
	gtk_window_set_titlebar (GTK_WINDOW (win), hbar);
	gtk_builder_connect_signals (builder, win);

	builder = gtk_builder_new_from_resource ("/org/gnome/parlatype/menus.ui");
	model = G_MENU_MODEL (gtk_builder_get_object (builder, "winmenu"));
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), model);
	g_object_unref (builder);

	/* Accels */
	GtkAccelGroup *accels;
	accels = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (win), accels);
	
	gtk_widget_add_accelerator (menu_button,
				"clicked",
				accels,
				GDK_KEY_F10,
				0,
				GTK_ACCEL_VISIBLE);
}

static void
pt_window_init (PtWindow *win)
{
	win->priv = pt_window_get_instance_private (win);

	gtk_widget_init_template (GTK_WIDGET (win));

	setup_settings (win);
	setup_player (win);
	setup_accels_actions_headerbar (win);
	setup_mediakeys (win);		/* this is in pt_mediakeys.c */
	pt_window_setup_dnd (win);	/* this is in pt_window_dnd.c */
	win->priv->recent = gtk_recent_manager_get_default ();
	win->priv->timer = 0;
}

static void
pt_window_dispose (GObject *object)
{
	PtWindow *win;
	win = PT_WINDOW (object);

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

	remove_timer (win);
	g_clear_object (&win->priv->editor);
	g_clear_object (&win->priv->proxy);
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
	case PROP_PAUSE:
		win->priv->pause = g_value_get_int (value);
		break;
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
	case PROP_PAUSE:
		g_value_set_int (value, win->priv->pause);
		break;
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
	G_OBJECT_CLASS (klass)->set_property = pt_window_set_property;
	G_OBJECT_CLASS (klass)->get_property = pt_window_get_property;
	G_OBJECT_CLASS (klass)->dispose = pt_window_dispose;
	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/parlatype/window.ui");
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, button_fast_back);	// not used
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, button_fast_forward);	// not used
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, button_jump_back);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, button_jump_forward);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, volumebutton);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, pos_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, dur_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, time_scale);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, time_adj);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PtWindow, speed_scale);

	obj_properties[PROP_PAUSE] =
	g_param_spec_int (
			"pause",
			"Seconds to rewind on pause",
			"Seconds to rewind on pause",
			0,	/* minimum */
			10,	/* maximum */
			0,
			G_PARAM_READWRITE);

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
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

PtWindow *
pt_window_new (PtApp *app)
{
	return g_object_new (PT_WINDOW_TYPE, "application", app, NULL);
}
