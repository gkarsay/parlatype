/* Copyright (C) Gabor Karsay 2016, 2019 <gabor.karsay@gmx.at>
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
#include "pt-mediakeys.h"

struct _PtMediakeysPrivate
{
	GDBusProxy *proxy;
	gint        dbus_watch_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtMediakeys, pt_mediakeys, PT_CONTROLLER_TYPE)

static void
on_media_key_pressed (GDBusProxy  *proxy,
                      gchar       *sender_name,
                      gchar       *signal_name,
                      GVariant    *parameters,
                      PtMediakeys *self)
{
	GVariant    *app_parameter;
	GVariant    *key_parameter;
	const gchar *app;
	const gchar *key;
	PtPlayer    *player = pt_controller_get_player (PT_CONTROLLER (self));

	if (g_strcmp0 (signal_name, "MediaPlayerKeyPressed") != 0)
		return;

	app_parameter = g_variant_get_child_value (parameters, 0);
	app = g_variant_get_string (app_parameter, NULL);
	if (g_strcmp0 (app, "Parlatype") != 0)
		return;
	
	key_parameter = g_variant_get_child_value (parameters, 1);
	key = g_variant_get_string (key_parameter, NULL);

	if (g_strcmp0 (key, "Play") == 0 || g_strcmp0 (key, "Pause") == 0 || g_strcmp0 (key, "Stop") == 0) {
		pt_player_play_pause (player);
	}

	if (g_strcmp0 (key, "Previous") == 0 || g_strcmp0 (key, "Rewind") == 0) {
		pt_player_jump_back (player);
		return;
	}

	if (g_strcmp0 (key, "Next") == 0 || g_strcmp0 (key, "FastForward") == 0) {
		pt_player_jump_forward (player);
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
grab_mediakeys (PtMediakeys *self)
{
	if (!self->priv->proxy)
		return;

	g_dbus_proxy_call (
			self->priv->proxy,
			"GrabMediaPlayerKeys",
			g_variant_new ("(su)", "Parlatype", 0),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			proxy_call_cb,
			NULL);	// user_data
}

static gboolean
window_focus_in_event_cb (GtkWidget     *widget,
                          GdkEventFocus *event,
                          PtMediakeys   *self)
{
	grab_mediakeys (self);
	return FALSE;
}

static void
remove_bus_watch (PtMediakeys *self)
{
	if (self->priv->dbus_watch_id != 0) {
		g_bus_unwatch_name (self->priv->dbus_watch_id);
		self->priv->dbus_watch_id = 0;
	}
}

static void
name_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  PtMediakeys     *self)
{
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	                  "MESSAGE", "Found %s", name);
	GError *error = NULL;

	self->priv->proxy = g_dbus_proxy_new_for_bus_sync (
			G_BUS_TYPE_SESSION,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			name,
			"/org/gnome/SettingsDaemon/MediaKeys",
			"org.gnome.SettingsDaemon.MediaKeys",
			NULL,
			&error);

	if (error) {
		g_warning ("Couldn’t create proxy for %s: %s", name, error->message);
		g_error_free (error);
		return;
	}

	grab_mediakeys (self);

	g_signal_connect (
			self->priv->proxy,
			"g-signal",
			G_CALLBACK (on_media_key_pressed),
			self);

	g_signal_connect (
			G_OBJECT (pt_controller_get_window (PT_CONTROLLER (self))),
			"focus-in-event",
			G_CALLBACK (window_focus_in_event_cb),
			self);

	remove_bus_watch (self);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  PtMediakeys     *self)
{
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Couldn’t find %s", name);

	if (self->priv->proxy) {
		g_object_unref (self->priv->proxy);
		self->priv->proxy = NULL;
	}
	remove_bus_watch (self);
}

void
pt_mediakeys_start (PtMediakeys *self)
{
	/* watch for GNOME settings daemon and stop watching immediatetly
	   after because we don’t assume this will change later on */
	self->priv->dbus_watch_id =
		g_bus_watch_name (
				G_BUS_TYPE_SESSION,
				"org.gnome.SettingsDaemon.MediaKeys",
				G_BUS_NAME_WATCHER_FLAGS_NONE,
				(GBusNameAppearedCallback) name_appeared_cb,
				(GBusNameVanishedCallback) name_vanished_cb,
				self,
				NULL);
}

static void
pt_mediakeys_init (PtMediakeys *self)
{
	self->priv = pt_mediakeys_get_instance_private (self);

	self->priv->proxy = NULL;
	self->priv->dbus_watch_id = 0;
}

static void
pt_mediakeys_dispose (GObject *object)
{
	PtMediakeys *self = PT_MEDIAKEYS (object);
	g_clear_object (&self->priv->proxy);
	G_OBJECT_CLASS (pt_mediakeys_parent_class)->dispose (object);
}

static void
pt_mediakeys_class_init (PtMediakeysClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = pt_mediakeys_dispose;
}

PtMediakeys *
pt_mediakeys_new (PtWindow *win)
{
	return g_object_new (PT_MEDIAKEYS_TYPE, "win", win, NULL);
}
