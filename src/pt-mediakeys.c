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
#include "pt-window.h"
#include "pt-window-private.h"


static void
on_media_key_pressed (GDBusProxy *proxy,
		      gchar      *sender_name,
		      gchar      *signal_name,
		      GVariant   *parameters,
		      PtWindow   *win)
{
	GVariant    *app_parameter;
	GVariant    *key_parameter;
	const gchar *app;
	const gchar *key;
	gboolean     state;

	if (g_strcmp0 (signal_name, "MediaPlayerKeyPressed") != 0)
		return;

	app_parameter = g_variant_get_child_value (parameters, 0);
	app = g_variant_get_string (app_parameter, NULL);
	if (g_strcmp0 (app, "Parlatype") != 0)
		return;
	
	key_parameter = g_variant_get_child_value (parameters, 1);
	key = g_variant_get_string (key_parameter, NULL);

	if (g_strcmp0 (key, "Play") == 0 || g_strcmp0 (key, "Pause") == 0 || g_strcmp0 (key, "Stop") == 0) {
		state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->priv->button_play), !state);
		return;
	}

	if (g_strcmp0 (key, "Previous") == 0 || g_strcmp0 (key, "Rewind") == 0) {
		gtk_button_clicked (GTK_BUTTON (win->priv->button_jump_back));
		return;
	}


	if (g_strcmp0 (key, "Next") == 0 || g_strcmp0 (key, "FastForward") == 0) {
		gtk_button_clicked (GTK_BUTTON (win->priv->button_jump_forward));
	}
}

static void
proxy_call_cb (GObject      *source_object,
	       GAsyncResult *res,
	       gpointer      user_data)
{
	GError *error = NULL;

	g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	
	if (error) {
		g_warning ("Failed to call media keys proxy: %s\n", error->message);
		g_error_free (error);
	}		
}

gboolean
window_state_event_cb (GtkWidget	    *widget,
		       GdkEventWindowState  *event,
		       PtWindow		    *win)
{
	if (!(event->new_window_state & GDK_WINDOW_STATE_FOCUSED)) {
		/* Window state changed to not focused.
		   We are not interested in that, return FALSE and the signal
		   can be processed further.
		   However, if we'd return TRUE, the window would always stay
		   in focus, it's colors would not change to unfocused state.
		   That might be desirable having the word processor and
		   parlatype open at the same time.
		   Maybe we offer that later as a user preference. */
		return FALSE;
	}

	if (win->priv->proxy) {
		g_dbus_proxy_call (
				win->priv->proxy,
				"GrabMediaPlayerKeys",
				g_variant_new ("(su)", "Parlatype", 0),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				proxy_call_cb,
				NULL);	// user_data
	}

	return FALSE;
}

void
setup_mediakeys (PtWindow *win)
{
	GError *error = NULL;

	win->priv->proxy = g_dbus_proxy_new_for_bus_sync (
			G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			"org.gnome.SettingsDaemon",
			"/org/gnome/SettingsDaemon/MediaKeys",
			"org.gnome.SettingsDaemon.MediaKeys",
			NULL,
			&error);

	if (error) {
		/* win->priv->proxy is now NULL */
		g_warning ("Couldn't grab media keys from GNOME Settings Daemon: %s\n", error->message);
		g_error_free (error);
		return;
	}

	g_signal_connect (
			win->priv->proxy,
			"g-signal",
			G_CALLBACK (on_media_key_pressed),
			win);
}
