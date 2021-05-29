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
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <parlatype.h>
#include "pt-window.h"
#include "pt-dbus-service.h"

struct _PtDbusServicePrivate
{
	gint      owner_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtDbusService, pt_dbus_service, PT_CONTROLLER_TYPE)

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.parlatype.Parlatype'>"
  "    <method name='GetTimestamp'>"
  "      <arg type='s' name='timestamp' direction='out'/>"
  "    </method>"
  "    <method name='GotoTimestamp'>"
  "      <arg type='s' name='timestamp' direction='in'/>"
  "    </method>"
  "    <method name='GetURI'>"
  "      <arg type='s' name='uri' direction='out'/>"
  "    </method>"
  "    <method name='PlayPause' />"
  "    <method name='JumpBack' />"
  "    <method name='JumpForward' />"
  "    <method name='IncreaseSpeed' />"
  "    <method name='DecreaseSpeed' />"
  "    <method name='TestHypothesisSignal'>"
  "      <arg type='s' name='message' direction='in'/>"
  "    </method>"
  "    <method name='TestFinalSignal'>"
  "      <arg type='s' name='message' direction='in'/>"
  "    </method>"
  "    <signal name='ASRHypothesis'>"
  "      <arg type='s' name='string' direction='out'/>"
  "    </signal>"
  "    <signal name='ASRFinal'>"
  "      <arg type='s' name='string' direction='out'/>"
  "    </signal>"
  "  </interface>"
  "</node>";

static void
player_asr_final_cb (PtPlayer *player,
                     gchar    *word,
                     gpointer  user_data)
{
	GDBusConnection *connection = G_DBUS_CONNECTION (user_data);
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       "/org/parlatype/parlatype",
				       "org.parlatype.Parlatype",
				       "ASRFinal",
				       g_variant_new ("(s)", word),
				       NULL);
}

static void
player_asr_hypothesis_cb (PtPlayer *player,
                          gchar    *word,
                          gpointer  user_data)
{
	GDBusConnection *connection = G_DBUS_CONNECTION (user_data);
	g_dbus_connection_emit_signal (connection,
				       NULL,
				       "/org/parlatype/parlatype",
				       "org.parlatype.Parlatype",
				       "ASRHypothesis",
				       g_variant_new ("(s)", word),
				       NULL);
}

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
	PtDbusService *self = PT_DBUS_SERVICE (user_data);
	PtPlayer      *player = pt_controller_get_player (PT_CONTROLLER (self));

	gchar	 *timestamp = NULL;
	gchar    *uri;

	if (g_strcmp0 (method_name, "GetTimestamp") == 0) {
		timestamp = pt_player_get_timestamp (player);
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			          "MESSAGE", "t: %s", timestamp);
		if (!timestamp)
			timestamp = "";
		g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("(s)", timestamp));
	} else if (g_strcmp0 (method_name, "GotoTimestamp") == 0) {
		g_variant_get (parameters, "(&s)", &timestamp);
		pt_player_goto_timestamp (player, timestamp);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "GetURI") == 0) {
		uri = pt_player_get_uri (player);
		if (!uri)
			uri = "";
		g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("(s)", uri));
	} else if (g_strcmp0 (method_name, "PlayPause") == 0) {
		pt_player_play_pause (player);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "JumpBack") == 0) {
		pt_player_jump_back (player);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "JumpForward") == 0) {
		pt_player_jump_forward (player);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "IncreaseSpeed") == 0) {
		gdouble value;
		g_object_get (player, "speed", &value, NULL);
		pt_player_set_speed (player, value + 0.1);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "DecreaseSpeed") == 0) {
		gdouble value;
		g_object_get (player, "speed", &value, NULL);
		pt_player_set_speed (player, value - 0.1);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "TestHypothesisSignal") == 0) {
		gchar *message;
		g_variant_get (parameters, "(&s)", &message);
		player_asr_hypothesis_cb (NULL, message, connection);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "TestFinalSignal") == 0) {
		gchar *message;
		g_variant_get (parameters, "(&s)", &message);
		player_asr_final_cb (NULL, message, connection);
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
	PtDbusService *self = PT_DBUS_SERVICE (user_data);
	PtPlayer      *player = pt_controller_get_player (PT_CONTROLLER (self));
	guint registration_id;

	registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/parlatype/parlatype",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       user_data,
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError */
	g_assert (registration_id > 0);

	g_signal_connect (player,
			"asr-final",
			G_CALLBACK (player_asr_final_cb),
			connection);

	g_signal_connect (player,
			"asr-hypothesis",
			G_CALLBACK (player_asr_hypothesis_cb),
			connection);
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
pt_dbus_service_start (PtDbusService *self)
{
	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (introspection_data != NULL);

	self->priv->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
			APP_ID,
			G_BUS_NAME_OWNER_FLAGS_NONE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost,
			self,
			NULL);
}

static void
pt_dbus_service_init (PtDbusService *self)
{
	self->priv = pt_dbus_service_get_instance_private (self);
	self->priv->owner_id = 0;
}

static void
pt_dbus_service_dispose (GObject *object)
{
	PtDbusService *self = PT_DBUS_SERVICE (object);

	if (self->priv->owner_id > 0) {
		g_bus_unown_name (self->priv->owner_id);
		g_dbus_node_info_unref (introspection_data);
		self->priv->owner_id = 0;
	}

	G_OBJECT_CLASS (pt_dbus_service_parent_class)->dispose (object);
}

static void
pt_dbus_service_class_init (PtDbusServiceClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = pt_dbus_service_dispose;
}

PtDbusService *
pt_dbus_service_new (PtWindow *win)
{
	return g_object_new (PT_DBUS_SERVICE_TYPE, "win", win, NULL);
}
