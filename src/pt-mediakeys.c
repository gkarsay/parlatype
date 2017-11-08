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
	GVariant *variant;
	GError *error = NULL;

	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	
	if (error) {
		g_warning ("Failed to call media keys proxy: %s", error->message);
		g_error_free (error);
	} else {
		g_variant_unref (variant);
	}
}

static void
grab_mediakeys (PtWindow *win)
{
	if (!win->priv->proxy)
		return;

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

static gboolean
window_focus_in_event_cb (GtkWidget	*widget,
		          GdkEventFocus *event,
		          PtWindow	*win)
{
	grab_mediakeys (win);
	return FALSE;
}

static void
remove_bus_watch (PtWindow *win)
{
	if (win->priv->dbus_watch_id != 0) {
		g_bus_unwatch_name (win->priv->dbus_watch_id);
		win->priv->dbus_watch_id = 0;
	}
}

static void
name_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  PtWindow        *win)
{
	g_debug ("Found org.gnome.SettingsDaemon");
	GError *error = NULL;

	win->priv->proxy = g_dbus_proxy_new_for_bus_sync (
			G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			"org.gnome.SettingsDaemon.MediaKeys",
			"/org/gnome/SettingsDaemon/MediaKeys",
			"org.gnome.SettingsDaemon.MediaKeys",
			NULL,
			&error);

	if (error) {
		g_warning ("Couldn't create proxy for org.gnome.SettingsDaemon: %s", error->message);
		g_error_free (error);
		return;
	}

	grab_mediakeys (win);

	g_signal_connect (
			win->priv->proxy,
			"g-signal",
			G_CALLBACK (on_media_key_pressed),
			win);

	g_signal_connect (
			G_OBJECT (win),
			"focus-in-event",
			G_CALLBACK (window_focus_in_event_cb),
			win);

	remove_bus_watch (win);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  PtWindow        *win)
{
	g_debug ("Couldn't find org.gnome.SettingsDaemon");
	if (win->priv->proxy) {
		g_object_unref (win->priv->proxy);
		win->priv->proxy = NULL;
	}
	remove_bus_watch (win);
}

void
setup_mediakeys (PtWindow *win)
{
	win->priv->proxy = NULL;
	win->priv->dbus_watch_id = 0;

	/* watch for GNOME settings daemon and stop watching immediatetly
	   after because we don't assume this will change later on */
	win->priv->dbus_watch_id =
		g_bus_watch_name (
				G_BUS_TYPE_SESSION,
				"org.gnome.SettingsDaemon.MediaKeys",
				G_BUS_NAME_WATCHER_FLAGS_NONE,
				(GBusNameAppearedCallback) name_appeared_cb,
				(GBusNameVanishedCallback) name_vanished_cb,
				win,
				NULL);
}
