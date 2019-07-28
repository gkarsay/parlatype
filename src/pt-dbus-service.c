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
#include <pt-player.h>
#include "pt-window.h"
#include "pt-dbus-service.h"

struct _PtDbusServicePrivate
{
	PtWindow *win;
	gint      owner_id;
};

enum
{
	PROP_0,
	PROP_WIN,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (PtDbusService, pt_dbus_service, G_TYPE_OBJECT)

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
		pt_player_play_pause (win->player);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "JumpBack") == 0) {
		pt_player_jump_back (win->player);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "JumpForward") == 0) {
		pt_player_jump_forward (win->player);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "IncreaseSpeed") == 0) {
		gdouble value;
		g_object_get (win->player, "speed", &value, NULL);
		pt_player_set_speed (win->player, value + 0.1);
		g_dbus_method_invocation_return_value (invocation, NULL);

	} else if (g_strcmp0 (method_name, "DecreaseSpeed") == 0) {
		gdouble value;
		g_object_get (win->player, "speed", &value, NULL);
		pt_player_set_speed (win->player, value + 0.1);
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
pt_dbus_service_start (PtDbusService *self)
{
	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (introspection_data != NULL);

	self->priv->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
			"com.github.gkarsay.parlatype",
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
pt_dbus_service_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	PtDbusService *self = PT_DBUS_SERVICE (object);

	switch (property_id) {
	case PROP_WIN:
		self->priv->win = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_dbus_service_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	PtDbusService *self = PT_DBUS_SERVICE (object);

	switch (property_id) {
	case PROP_WIN:
		g_value_set_object (value, self->priv->win);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
static void
pt_dbus_service_class_init (PtDbusServiceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = pt_dbus_service_set_property;
	object_class->get_property = pt_dbus_service_get_property;
	object_class->dispose      = pt_dbus_service_dispose;

	obj_properties[PROP_WIN] =
	g_param_spec_object ("win",
                             "Parent PtWindow",
                             "Parent PtWindow",
                             PT_WINDOW_TYPE,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (
			object_class,
			N_PROPERTIES,
			obj_properties);
}

PtDbusService *
pt_dbus_service_new (PtWindow *win)
{
	return g_object_new (PT_DBUS_SERVICE_TYPE, "win", win, NULL);
}
