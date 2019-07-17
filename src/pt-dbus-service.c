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
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <pt-player.h>
#include "pt-window.h"
#include "pt-window-private.h"


static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='com.github.gkarsay.parlatype'>"
  "    <method name='GetTimestamp'>"
  "      <arg type='s' name='timestamp' direction='out'/>"
  "    </method>"
  "    <method name='GotoTimestamp'>"
  "      <arg type='s' name='timestamp' direction='in'/>"
  "    </method>"
  "    <method name='PlayPause' />"
  "    <method name='JumpBack' />"
  "    <method name='JumpForward' />"
  "    <method name='IncreaseSpeed' />"
  "    <method name='DecreaseSpeed' />"
  "  </interface>"
  "</node>";

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	PtWindow *win;
	win = PT_WINDOW (user_data);

	gchar	 *timestamp = NULL;
	
	if (g_strcmp0 (method_name, "GetTimestamp") == 0) {
		timestamp = pt_player_get_timestamp (win->player);
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			          "MESSAGE", "t: %s", timestamp);
		if (!timestamp)
			timestamp = "";
		g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("(s)", timestamp));
	} else if (g_strcmp0 (method_name, "GotoTimestamp") == 0) {
		g_variant_get (parameters, "(&s)", &timestamp);
		pt_player_goto_timestamp (win->player, timestamp);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "PlayPause") == 0) {
		gboolean state;
		state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->priv->button_play));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->priv->button_play), !state);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "JumpBack") == 0) {
		gtk_button_clicked (GTK_BUTTON (win->priv->button_jump_back));
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "JumpForward") == 0) {
		gtk_button_clicked (GTK_BUTTON (win->priv->button_jump_forward));
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "IncreaseSpeed") == 0) {
		gdouble value;
		value = gtk_range_get_value (GTK_RANGE (win->priv->speed_scale));
		gtk_range_set_value (GTK_RANGE (win->priv->speed_scale), value + 0.1);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "DecreaseSpeed") == 0) {
		gdouble value;
		value = gtk_range_get_value (GTK_RANGE (win->priv->speed_scale));
		gtk_range_set_value (GTK_RANGE (win->priv->speed_scale), value - 0.1);
		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
	handle_method_call,
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	guint registration_id;

	registration_id = g_dbus_connection_register_object (connection,
                                                       "/com/github/gkarsay/parlatype",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       user_data,
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError */
	g_assert (registration_id > 0);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
}

void
setup_dbus_service (PtWindow *win)
{
	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (introspection_data != NULL);

	win->priv->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
			"com.github.gkarsay.parlatype",
			G_BUS_NAME_OWNER_FLAGS_NONE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost,
			win,
			NULL);
}

void
close_dbus_service (PtWindow *win)
{
	if (win->priv->owner_id > 0) {
		g_bus_unown_name (win->priv->owner_id);
		g_dbus_node_info_unref (introspection_data);
		win->priv->owner_id = 0;
	}
}
