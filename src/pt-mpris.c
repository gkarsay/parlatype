/* Copyright (C) 2010  Jonathan Matthew  <jonathan@d14n.org>
 * Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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
 *
 * pt-mpris.c is taken from rhythmbox/plugins/mpris/rb-mpris-plugin.c
 * and slightly adapted for Parlatype.
 */

#include "pt-mpris.h"
#include "config.h"
#include "pt-window.h"
#include <parlatype.h>

#include "mpris-spec.h"

#define ENTRY_OBJECT_PATH_PREFIX "/org/mpris/MediaPlayer2/Track/"

struct _PtMprisPrivate
{

  GDBusConnection *connection;
  GDBusNodeInfo *node_info;
  guint name_own_id;
  guint root_id;
  guint player_id;

  GHashTable *player_property_changes;
  guint property_emit_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtMpris, pt_mpris, PT_CONTROLLER_TYPE)

static void
emit_property_changes (PtMpris *self, GHashTable *changes, const char *interface)
{
  GError *error = NULL;
  GVariantBuilder *properties;
  GVariantBuilder *invalidated;
  GVariant *parameters;
  gpointer propname, propvalue;
  GHashTableIter iter;

  /* build property changes */
  properties = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  invalidated = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  g_hash_table_iter_init (&iter, changes);
  while (g_hash_table_iter_next (&iter, &propname, &propvalue))
    {
      if (propvalue != NULL)
        {
          g_variant_builder_add (properties,
                                 "{sv}",
                                 propname,
                                 propvalue);
        }
      else
        {
          g_variant_builder_add (invalidated, "s", propname);
        }
    }

  parameters = g_variant_new ("(sa{sv}as)",
                              interface,
                              properties,
                              invalidated);
  g_variant_builder_unref (properties);
  g_variant_builder_unref (invalidated);
  g_dbus_connection_emit_signal (self->priv->connection,
                                 NULL,
                                 MPRIS_OBJECT_NAME,
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 parameters,
                                 &error);
  if (error != NULL)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Unable to send MPRIS property changes for %s: %s",
                        interface, error->message);
      g_clear_error (&error);
    }
}

static gboolean
emit_properties_idle (PtMpris *self)
{
  if (self->priv->player_property_changes != NULL)
    {
      emit_property_changes (self, self->priv->player_property_changes, MPRIS_PLAYER_INTERFACE);
      g_hash_table_destroy (self->priv->player_property_changes);
      self->priv->player_property_changes = NULL;
    }

  self->priv->property_emit_id = 0;
  return FALSE;
}

static void
add_player_property_change (PtMpris *self,
                            const char *property,
                            GVariant *value)
{
  if (self->priv->player_property_changes == NULL)
    {
      self->priv->player_property_changes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
    }
  g_hash_table_insert (self->priv->player_property_changes, g_strdup (property), g_variant_ref_sink (value));

  if (self->priv->property_emit_id == 0)
    {
      self->priv->property_emit_id = g_idle_add ((GSourceFunc) emit_properties_idle, self);
    }
}

static void
handle_root_method_call (GDBusConnection *connection,
                         const char *sender,
                         const char *object_path,
                         const char *interface_name,
                         const char *method_name,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation,
                         PtMpris *self)
{
  PtWindow *window = pt_controller_get_window (PT_CONTROLLER (self));

  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_ROOT_INTERFACE) != 0)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_NOT_SUPPORTED,
                                             "Method %s.%s not supported",
                                             interface_name,
                                             method_name);
      return;
    }

  if (g_strcmp0 (method_name, "Raise") == 0)
    {
      gtk_window_present_with_time (GTK_WINDOW (window), GDK_CURRENT_TIME);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "Quit") == 0)
    {
      gtk_window_destroy (GTK_WINDOW (window));
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_NOT_SUPPORTED,
                                             "Method %s.%s not supported",
                                             interface_name,
                                             method_name);
    }
}

static GVariant *
get_root_property (GDBusConnection *connection,
                   const char *sender,
                   const char *object_path,
                   const char *interface_name,
                   const char *property_name,
                   GError **error,
                   PtMpris *self)
{
  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_ROOT_INTERFACE) != 0)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NOT_SUPPORTED,
                   "Property %s.%s not supported",
                   interface_name,
                   property_name);
      return NULL;
    }

  if (g_strcmp0 (property_name, "CanQuit") == 0)
    {
      return g_variant_new_boolean (TRUE);
    }
  else if (g_strcmp0 (property_name, "CanRaise") == 0)
    {
      return g_variant_new_boolean (TRUE);
    }
  else if (g_strcmp0 (property_name, "HasTrackList") == 0)
    {
      return g_variant_new_boolean (FALSE);
    }
  else if (g_strcmp0 (property_name, "Identity") == 0)
    {
      return g_variant_new_string ("Parlatype");
    }
  else if (g_strcmp0 (property_name, "DesktopEntry") == 0)
    {
      return g_variant_new_string (APP_ID);
    }
  else if (g_strcmp0 (property_name, "SupportedUriSchemes") == 0)
    {
      /* only file allowed currently, but not planning to support this later seriously */
      const char *fake_supported_schemes[] = {
        "file", NULL
      };
      return g_variant_new_strv (fake_supported_schemes, -1);
    }
  else if (g_strcmp0 (property_name, "SupportedMimeTypes") == 0)
    {
      /* nor this */
      const char *fake_supported_mimetypes[] = {
        "application/ogg", "audio/x-vorbis+ogg", "audio/x-flac", "audio/mpeg", NULL
      };
      return g_variant_new_strv (fake_supported_mimetypes, -1);
    }

  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_NOT_SUPPORTED,
               "Property %s.%s not supported",
               interface_name,
               property_name);
  return NULL;
}

static const GDBusInterfaceVTable root_vtable = {
  (GDBusInterfaceMethodCallFunc) handle_root_method_call,
  (GDBusInterfaceGetPropertyFunc) get_root_property,
  NULL
};

static void
handle_result (GDBusMethodInvocation *invocation, gboolean ret, GError *error)
{
  if (ret)
    {
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else
    {
      if (error != NULL)
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "returning error: %s", error->message);
          g_dbus_method_invocation_return_gerror (invocation, error);
          g_error_free (error);
        }
      else
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "returning unknown error");
          g_dbus_method_invocation_return_error_literal (invocation,
                                                         G_DBUS_ERROR,
                                                         G_DBUS_ERROR_FAILED,
                                                         "Unknown error");
        }
    }
}

static void
build_track_metadata (PtMpris *self,
                      GVariantBuilder *builder,
                      PtPlayer *player)
{
  gchar *uri;

  uri = pt_player_get_uri (player);

  if (!uri)
    {
      g_variant_builder_add (builder, "{sv}", "mpris:trackid",
                             g_variant_new ("o", "/org/mpris/MediaPlayer2/TrackList/NoTrack"));
      return;
    }

  g_variant_builder_add (builder, "{sv}", "mpris:trackid",
                         g_variant_new ("o", "/org/mpris/MediaPlayer2/Track/1"));
  g_variant_builder_add (builder, "{sv}", "xesam:url",
                         g_variant_new ("s", uri));
  g_variant_builder_add (builder, "{sv}", "mpris:length",
                         g_variant_new_int64 (pt_player_get_duration (player) / 1000));
  g_free (uri);
}

static void
handle_player_method_call (GDBusConnection *connection,
                           const char *sender,
                           const char *object_path,
                           const char *interface_name,
                           const char *method_name,
                           GVariant *parameters,
                           GDBusMethodInvocation *invocation,
                           PtMpris *self)

{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  GError *error = NULL;
  gboolean ret;

  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_PLAYER_INTERFACE) != 0)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_NOT_SUPPORTED,
                                             "Method %s.%s not supported",
                                             interface_name,
                                             method_name);
      return;
    }

  if (g_strcmp0 (method_name, "Next") == 0)
    {
      /* CanGoNext is always false */
      handle_result (invocation, TRUE, error);
    }
  else if (g_strcmp0 (method_name, "Previous") == 0)
    {
      /* CanGoPrevious is always false */
      handle_result (invocation, TRUE, error);
    }
  else if (g_strcmp0 (method_name, "Pause") == 0)
    {
      pt_player_pause (player);
      handle_result (invocation, TRUE, error);
    }
  else if (g_strcmp0 (method_name, "PlayPause") == 0)
    {
      pt_player_play_pause (player);
      handle_result (invocation, TRUE, error);
    }
  else if (g_strcmp0 (method_name, "Stop") == 0)
    {
      pt_player_pause (player);
      pt_player_jump_to_position (player, 0);
      handle_result (invocation, TRUE, NULL);
    }
  else if (g_strcmp0 (method_name, "Play") == 0)
    {
      pt_player_play (player);
      handle_result (invocation, TRUE, error);
    }
  else if (g_strcmp0 (method_name, "Seek") == 0)
    {
      gint64 offset;
      g_variant_get (parameters, "(x)", &offset);
      pt_player_jump_relative (player, offset / 1000);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "SetPosition") == 0)
    {
      gint64 position;
      const char *trackid;

      g_variant_get (parameters, "(&ox)", &trackid, &position);

      if (g_str_has_prefix (trackid, ENTRY_OBJECT_PATH_PREFIX) == FALSE)
        {
          /* this can't possibly be the current playing track, so ignore it */
          g_dbus_method_invocation_return_value (invocation, NULL);
          return;
        }

      trackid += strlen (ENTRY_OBJECT_PATH_PREFIX);

      if (g_strcmp0 (trackid, "1") != 0)
        {
          /* client got the wrong entry, ignore it */
          g_dbus_method_invocation_return_value (invocation, NULL);
          return;
        }

      pt_player_jump_to_position (player, position / 1000);
      handle_result (invocation, TRUE, NULL);
    }
  else if (g_strcmp0 (method_name, "OpenUri") == 0)
    {
      char *uri;

      g_variant_get (parameters, "(&s)", &uri);
      ret = pt_player_open_uri (player, uri);
      handle_result (invocation, ret, NULL);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_NOT_SUPPORTED,
                                             "Method %s.%s not supported",
                                             interface_name,
                                             method_name);
    }
}

static GVariant *
get_playback_status (PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  PtStateType state;

  g_object_get (player, "state", &state, NULL);

  switch (state)
    {
    case (PT_STATE_PLAYING):
      return g_variant_new_string ("Playing");
    case (PT_STATE_PAUSED):
      return g_variant_new_string ("Paused");
    case (PT_STATE_STOPPED):
      return g_variant_new_string ("Stopped");
    default:
      return NULL;
    }
}

static GVariant *
get_player_property (GDBusConnection *connection,
                     const char *sender,
                     const char *object_path,
                     const char *interface_name,
                     const char *property_name,
                     GError **error,
                     PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  PtStateType state;

  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_PLAYER_INTERFACE) != 0)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NOT_SUPPORTED,
                   "Property %s.%s not supported",
                   interface_name,
                   property_name);
      return NULL;
    }

  if (g_strcmp0 (property_name, "PlaybackStatus") == 0)
    {
      return get_playback_status (self);
    }
  else if (g_strcmp0 (property_name, "Rate") == 0)
    {
      return g_variant_new_double (pt_player_get_speed (player));
    }
  else if (g_strcmp0 (property_name, "Metadata") == 0)
    {
      GVariantBuilder *builder;
      GVariant *v;

      builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
      build_track_metadata (self, builder, player);
      v = g_variant_builder_end (builder);
      g_variant_builder_unref (builder);
      return v;
    }
  else if (g_strcmp0 (property_name, "Volume") == 0)
    {
      return g_variant_new_double (pt_player_get_volume (player));
    }
  else if (g_strcmp0 (property_name, "Position") == 0)
    {
      gint64 t;
      t = pt_player_get_position (player);
      if (t != -1)
        {
          return g_variant_new_int64 ((gint64) t * 1000);
        }
      else
        {
          g_set_error (error,
                       G_DBUS_ERROR,
                       G_DBUS_ERROR_NOT_SUPPORTED,
                       "Property %s.%s not supported",
                       interface_name,
                       property_name);
          return NULL;
        }
    }
  else if (g_strcmp0 (property_name, "MinimumRate") == 0)
    {
      return g_variant_new_double (0.5);
    }
  else if (g_strcmp0 (property_name, "MaximumRate") == 0)
    {
      return g_variant_new_double (2.0);
    }
  else if (g_strcmp0 (property_name, "CanGoNext") == 0)
    {
      return g_variant_new_boolean (FALSE);
    }
  else if (g_strcmp0 (property_name, "CanGoPrevious") == 0)
    {
      return g_variant_new_boolean (FALSE);
    }
  else if (g_strcmp0 (property_name, "CanPlay") == 0)
    {
      g_object_get (player, "state", &state, NULL);
      if (state == PT_STATE_STOPPED)
        return g_variant_new_boolean (FALSE);
      else
        return g_variant_new_boolean (TRUE);
    }
  else if (g_strcmp0 (property_name, "CanPause") == 0)
    {
      g_object_get (player, "state", &state, NULL);
      if (state == PT_STATE_STOPPED)
        return g_variant_new_boolean (FALSE);
      else
        return g_variant_new_boolean (TRUE);
    }
  else if (g_strcmp0 (property_name, "CanSeek") == 0)
    {
      g_object_get (player, "state", &state, NULL);
      if (state == PT_STATE_STOPPED)
        return g_variant_new_boolean (FALSE);
      else
        return g_variant_new_boolean (TRUE);
    }
  else if (g_strcmp0 (property_name, "CanControl") == 0)
    {
      return g_variant_new_boolean (TRUE);
    }

  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_NOT_SUPPORTED,
               "Property %s.%s not supported",
               interface_name,
               property_name);
  return NULL;
}

static gboolean
set_player_property (GDBusConnection *connection,
                     const char *sender,
                     const char *object_path,
                     const char *interface_name,
                     const char *property_name,
                     GVariant *value,
                     GError **error,
                     PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));

  if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
      g_strcmp0 (interface_name, MPRIS_PLAYER_INTERFACE) != 0)
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NOT_SUPPORTED,
                   "%s:%s not supported",
                   object_path,
                   interface_name);
      return FALSE;
    }

  if (g_strcmp0 (property_name, "Rate") == 0)
    {
      pt_player_set_speed (player, g_variant_get_double (value));
      return TRUE;
    }
  else if (g_strcmp0 (property_name, "Volume") == 0)
    {
      pt_player_set_volume (player, g_variant_get_double (value));
      return TRUE;
    }

  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_NOT_SUPPORTED,
               "Property %s.%s not supported",
               interface_name,
               property_name);
  return FALSE;
}

static const GDBusInterfaceVTable player_vtable = {
  (GDBusInterfaceMethodCallFunc) handle_player_method_call,
  (GDBusInterfaceGetPropertyFunc) get_player_property,
  (GDBusInterfaceSetPropertyFunc) set_player_property,
};

static void
state_changed_cb (GObject *object, GParamSpec *pspec, PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  PtStateType state;

  g_object_get (player, "state", &state, NULL);
  add_player_property_change (self, "PlaybackStatus",
                              get_playback_status (self));
  add_player_property_change (self, "CanPlay",
                              g_variant_new_boolean (state != PT_STATE_STOPPED));
  add_player_property_change (self, "CanPause",
                              g_variant_new_boolean (state != PT_STATE_STOPPED));
}

static void
speed_changed_cb (GObject *object, GParamSpec *pspec, PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  add_player_property_change (self, "Rate",
                              g_variant_new_double (pt_player_get_speed (player)));
}

static void
volume_changed_cb (GObject *object, GParamSpec *pspec, PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  add_player_property_change (self, "Volume",
                              g_variant_new_double (pt_player_get_volume (player)));
}

static void
uri_changed_cb (GObject *object, GParamSpec *pspec, PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  GVariantBuilder *builder;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  build_track_metadata (self, builder, player);
  add_player_property_change (self, "Metadata",
                              g_variant_builder_end (builder));
  g_variant_builder_unref (builder);
}

static void
name_acquired_cb (GDBusConnection *connection, const char *name, PtMpris *self)
{
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "successfully acquired dbus name %s", name);
}

static void
name_lost_cb (GDBusConnection *connection, const char *name, PtMpris *self)
{
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "lost dbus name %s", name);
}

void
pt_mpris_start (PtMpris *self)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  GDBusInterfaceInfo *ifaceinfo;
  GError *error = NULL;

  self->priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (error != NULL)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Unable to connect to D-Bus session bus: %s", error->message);
      return;
    }

  /* parse introspection data */
  self->priv->node_info = g_dbus_node_info_new_for_xml (mpris_introspection_xml, &error);
  if (error != NULL)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Unable to read MPRIS interface specification: %s", error->message);
      return;
    }

  /* register root interface */
  ifaceinfo = g_dbus_node_info_lookup_interface (self->priv->node_info, MPRIS_ROOT_INTERFACE);
  self->priv->root_id = g_dbus_connection_register_object (self->priv->connection,
                                                           MPRIS_OBJECT_NAME,
                                                           ifaceinfo,
                                                           &root_vtable,
                                                           self,
                                                           NULL,
                                                           &error);
  if (error != NULL)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Unable to register MPRIS root interface: %s", error->message);
      g_error_free (error);
    }

  /* register player interface */
  ifaceinfo = g_dbus_node_info_lookup_interface (self->priv->node_info, MPRIS_PLAYER_INTERFACE);
  self->priv->player_id = g_dbus_connection_register_object (self->priv->connection,
                                                             MPRIS_OBJECT_NAME,
                                                             ifaceinfo,
                                                             &player_vtable,
                                                             self,
                                                             NULL,
                                                             &error);
  if (error != NULL)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Unable to register MPRIS player interface: %s", error->message);
      g_error_free (error);
    }

  g_signal_connect_object (player,
                           "notify::volume",
                           G_CALLBACK (volume_changed_cb),
                           self, 0);

  g_signal_connect_object (player,
                           "notify::state",
                           G_CALLBACK (state_changed_cb),
                           self, 0);

  g_signal_connect_object (player,
                           "notify::speed",
                           G_CALLBACK (speed_changed_cb),
                           self, 0);

  g_signal_connect_object (player,
                           "notify::current-uri",
                           G_CALLBACK (uri_changed_cb),
                           self, 0);

  self->priv->name_own_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                            MPRIS_BUS_NAME_PREFIX ".parlatype",
                                            G_BUS_NAME_OWNER_FLAGS_NONE,
                                            NULL,
                                            (GBusNameAcquiredCallback) name_acquired_cb,
                                            (GBusNameLostCallback) name_lost_cb,
                                            g_object_ref (self),
                                            g_object_unref);
}

static void
pt_mpris_init (PtMpris *self)
{
  self->priv = pt_mpris_get_instance_private (self);
}

static void
pt_mpris_dispose (GObject *object)
{
  PtMpris *self = PT_MPRIS (object);
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));

  if (self->priv->root_id != 0)
    {
      g_dbus_connection_unregister_object (self->priv->connection, self->priv->root_id);
      self->priv->root_id = 0;
    }
  if (self->priv->player_id != 0)
    {
      g_dbus_connection_unregister_object (self->priv->connection, self->priv->player_id);
      self->priv->player_id = 0;
    }

  g_signal_handlers_disconnect_by_func (player,
                                        G_CALLBACK (volume_changed_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (player,
                                        G_CALLBACK (state_changed_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (player,
                                        G_CALLBACK (speed_changed_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (player,
                                        G_CALLBACK (uri_changed_cb),
                                        self);

  G_OBJECT_CLASS (pt_mpris_parent_class)->dispose (object);
}

static void
pt_mpris_class_init (PtMprisClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = pt_mpris_dispose;
}

PtMpris *
pt_mpris_new (PtWindow *win)
{
  return g_object_new (PT_MPRIS_TYPE, "win", win, NULL);
}
