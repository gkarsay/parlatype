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
#include <gst/gst.h>
#include "pt-player.h"

struct _PtPlayerPrivate
{
	GstElement *pipeline;
	GstElement *source;
	GstElement *volume;
	GstElement *audio;
	guint	    bus_watch_id;

	gint64	    dur;
	gdouble	    speed;
};

enum
{
	PROP_0,
	PROP_SPEED,
	PROP_VOLUME,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

#define METADATA_POSITION "metadata::parlatype::position"

static void pt_player_initable_iface_init (GInitableIface *iface);
static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);

G_DEFINE_TYPE_WITH_CODE (PtPlayer, pt_player, G_TYPE_OBJECT,
			 G_ADD_PRIVATE (PtPlayer)
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						pt_player_initable_iface_init))


static gboolean
pt_player_query_position (PtPlayer *player,
			  gpointer  position)
{
	gboolean result;
	result = gst_element_query_position (player->priv->pipeline, GST_FORMAT_TIME, position);
	return result;
}

static gboolean
pt_player_query_duration (PtPlayer *player,
			  gpointer  position)
{
	gboolean result;
	result = gst_element_query_duration (player->priv->pipeline, GST_FORMAT_TIME, position);
	return result;
}

static void
pt_player_clear (PtPlayer *player)
{
	gst_element_set_state (player->priv->pipeline, GST_STATE_NULL);
}

static void
pt_player_seek (PtPlayer *player,
		gint64    position)
{
	gst_element_seek (
		player->priv->pipeline,
		player->priv->speed,
		GST_FORMAT_TIME,
		GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
		GST_SEEK_TYPE_SET,
		position,
		GST_SEEK_TYPE_NONE,
		-1);
}

static GFile*
pt_player_get_file (PtPlayer *player)
{
	const gchar *location = NULL;

	g_object_get (G_OBJECT (player->priv->source), "location", &location, NULL);

	if (location)
		return g_file_new_for_path (location);
	else
		return NULL;
}

static void
metadata_save_position (PtPlayer *player)
{
	GError	  *error = NULL;
	GFile	  *file = NULL;
	GFileInfo *info;
	gint64     pos;
	gchar	   value[64];

	file = pt_player_get_file (player);
	if (!file)
		return;

	if (!pt_player_query_position (player, &pos))
		return;
	
	pos = pos / GST_MSECOND;

	info = g_file_info_new ();
	g_snprintf (value, sizeof (value), "%ld", pos);

	g_file_info_set_attribute_string (info, METADATA_POSITION, value);

	g_file_set_attributes_from_info (
			file,
			info,
			G_FILE_QUERY_INFO_NONE,
			NULL,
			&error);
	
	if (error) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
	}

	g_debug ("metadata: position saved");
	g_object_unref (info);
}

static void
metadata_get_position (PtPlayer *player)
{
	GError	  *error = NULL;
	GFile	  *file = NULL;
	GFileInfo *info;
	gchar	  *value = NULL;
	gint64     pos = 0;

	file = pt_player_get_file (player);
	if (!file)
		return;

	info = g_file_query_info (file, METADATA_POSITION, G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (error) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		g_object_unref (file);
		return;
	}

	value = g_file_info_get_attribute_as_string (info, METADATA_POSITION);
	if (value) {
		pos = g_ascii_strtoull (value, NULL, 0);
		g_free (value);

		if (pos > 0) {
			g_debug ("metadata: got position");
		}
	}
		
	/* Set either to position or 0 */
	pt_player_jump_to_position (player, pos);

	g_object_unref (file);
	g_object_unref (info);
}

void
pt_player_pause (PtPlayer *player)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gst_element_set_state (player->priv->pipeline, GST_STATE_PAUSED);
}

void
pt_player_play (PtPlayer *player)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gst_element_set_state (player->priv->pipeline, GST_STATE_PLAYING);
}


static gboolean
open_file_bus_handler (GstBus     *bus,
		       GstMessage *msg,
		       gpointer    data)
{
	GTask    *task = (GTask *) data;
	PtPlayer *player = g_task_get_source_object (task);
	GstState  state;
	gint64    pos = 0;
	
	//g_debug ("Message: %s; sent by: %s", GST_MESSAGE_TYPE_NAME (msg), GST_MESSAGE_SRC_NAME (msg));

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ERROR: {
		gchar    *debug;
		GError   *error;
		gst_message_parse_error (msg, &error, &debug);
		g_debug ("ERROR from element %s: %s", GST_OBJECT_NAME (msg->src), error->message);
		g_debug ("Debugging info: %s", (debug) ? debug : "none");
		g_free (debug);
		g_task_return_error (task, error);
		pt_player_clear (player);
		return FALSE;
		}

	case GST_MESSAGE_DURATION_CHANGED:
		
	case GST_MESSAGE_ASYNC_DONE:
		pt_player_query_duration (player, &pos);
		gst_element_get_state (player->priv->pipeline, &state, NULL, -1);
		if (pos > 0 && state == GST_STATE_PLAYING) {
			g_task_return_boolean (task, TRUE);
			g_object_unref (task);
			return FALSE;
		}
			
	default:
		break;
	}

	return TRUE;
}

typedef struct
{
	GAsyncResult *res;
	GMainLoop    *loop;
} SyncData;

static void
quit_loop_cb (PtPlayer	   *player,
	      GAsyncResult *res,
	      gpointer      user_data)
{
	SyncData *data = user_data;
	data->res = g_object_ref (res);
	g_main_loop_quit (data->loop);
}

static gboolean
pt_player_open_file_finish (PtPlayer      *player,
			    GAsyncResult  *result,
			    GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (result, player), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
pt_player_open_file_async (PtPlayer	       *player,
			   gchar	       *uri,
			   GCancellable	       *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer		user_data)
{
	GTask  *task;
	GFile  *file;
	gchar  *location = NULL;
	GstBus *bus;

	task = g_task_new (player, cancellable, callback, user_data);

	/* Change uri to location */
	file = g_file_new_for_uri (uri);
	location = g_file_get_path (file);
	g_object_unref (file);
	
	if (!location) {
		g_task_return_new_error (
				task,
				GST_RESOURCE_ERROR,
				GST_RESOURCE_ERROR_NOT_FOUND,
				_("URI not valid: %s"), uri);

		g_object_unref (task);
		g_main_loop_quit (user_data);
		return;
	}

	/* If we had an open file before, remember its position */
	metadata_save_position (player);

	/* Reset any open streams and set new location*/
	pt_player_clear (player);
	g_object_set (G_OBJECT (player->priv->source), "location", location, NULL);
	g_free (location);

	/* setup message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (player->priv->pipeline));
	player->priv->bus_watch_id = gst_bus_add_watch (bus, open_file_bus_handler, task);
	gst_object_unref (bus);

	player->priv->dur = -1;
	pt_player_play (player);
}

/* Reference for myself: async stuff is modeled on: https://git.gnome.org/browse/glib/tree/gio/gdbusconnection.c
   g_dbus_connection_send_message_with_reply_sync() */
gboolean
pt_player_open_uri (PtPlayer  *player,
		    gchar     *uri,
		    GError   **error)
{
	/* A file is opened. We play it until we get a duration-changed signal,
	   blocking in a g_main_loop. Reason: On some files PAUSED state is not
	   enough to get a duration, even Playing and setting to PAUSED
	   immediately might be not enough. We are really waiting until we have
	   a duration.
	   That's why we mute the volume and reset it later. In the end we
	   pause the player and look for the last known position in metadata.
	   This sets it to position 0 if no metadata is found. */

	g_return_val_if_fail (PT_IS_PLAYER (player), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	g_debug ("open uri");

	gboolean      result;
	SyncData      data;
	GMainContext *context;
	GstBus	     *bus;
	guint	      bus_watch_id;

	/* Only one message bus possible, we remove any previous bus */
	if (player->priv->bus_watch_id > 0)
		g_source_remove (player->priv->bus_watch_id);

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);
	
	data.loop = g_main_loop_new (context, FALSE);
	data.res = NULL;

	pt_player_mute_volume (player, TRUE);
	pt_player_open_file_async (player, uri, NULL, (GAsyncReadyCallback) quit_loop_cb, &data);
	g_main_loop_run (data.loop);

	
	result = pt_player_open_file_finish (player, data.res, error);

	g_main_loop_unref (data.loop);
	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);
	if (data.res)
		g_object_unref (data.res);

	if (result) {
		bus = gst_pipeline_get_bus (GST_PIPELINE (player->priv->pipeline));
		player->priv->bus_watch_id = gst_bus_add_watch (bus, bus_call, player);
		gst_object_unref (bus);

		pt_player_query_duration (player, &player->priv->dur);
		pt_player_pause (player);
		metadata_get_position (player);
	}

	pt_player_mute_volume (player, FALSE);

	return result;
}

void
pt_player_jump_relative (PtPlayer *player,
			 gint      milliseconds)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gint64 pos, new;

	if (!pt_player_query_position (player, &pos))
		return;
	
	new = pos + GST_MSECOND * (gint64) milliseconds;
	g_debug ("jump relative:\ndur = %ld\nnew = %ld", player->priv->dur, new);

	if (new > player->priv->dur)
		new = player->priv->dur;

	if (new < 0)
		new = 0;

	pt_player_seek (player, new);
}

void
pt_player_jump_to_position (PtPlayer *player,
			    gint      milliseconds)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gint64 pos;

	pos = GST_MSECOND * (gint64) milliseconds;

	if (pos > player->priv->dur || pos < 0) {
		g_debug ("jump to position failed\ndur = %ld\npos = %ld", player->priv->dur, pos);
		return;
	}

	pt_player_seek (player, pos);
}

void
pt_player_jump_to_permille (PtPlayer *player,
			    guint     permille)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	g_return_if_fail (permille <= 1000);

	gint64 new;

	new = player->priv->dur * (gint64) permille / 1000;
	pt_player_seek (player, new);
}

gint
pt_player_get_permille (PtPlayer *player)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), -1);

	gint64 pos;
	gfloat frac;

	if (!pt_player_query_position (player, &pos))
		return -1;

	return (gfloat) pos / (gfloat) player->priv->dur * 1000;
}

/* Setting this on the player's "speed" property is missing the seek event */
void
pt_player_set_speed (PtPlayer *player,
		     gdouble   speed)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	g_return_if_fail (speed >= 0);

	gint64 pos;

	if (!pt_player_query_position (player, &pos))
		return;

	player->priv->speed = speed;
	pt_player_seek (player, pos);
}

/* This can also be set on the player's "volume" property */
void
pt_player_set_volume (PtPlayer *player,
		      gdouble   volume)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	g_return_if_fail (volume >= 0 && volume <= 1);

	g_object_set (G_OBJECT (player->priv->volume), "volume", volume, NULL);
}

/* Remembers volume level, we don't have to keep track of the old value */
void
pt_player_mute_volume (PtPlayer *player,
		       gboolean  mute)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	g_object_set (G_OBJECT (player->priv->volume), "mute", mute, NULL);
}

void
pt_player_rewind (PtPlayer *player,
		  gdouble   speed)
{
	/* FIXME Doesn't work at all!!! */

	g_return_if_fail (PT_IS_PLAYER (player));

	gint64 pos;

	if (!pt_player_query_position (player, &pos))
		return;

	gst_element_seek (
		player->priv->pipeline,
		-1.0,
		GST_FORMAT_TIME,
		GST_SEEK_FLAG_SKIP,
		GST_SEEK_TYPE_NONE,
		0,
		GST_SEEK_TYPE_SET,
		pos);

	pt_player_play (player);
}

void
pt_player_fast_forward (PtPlayer *player,
			gdouble   speed)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gint64 pos;

	if (!pt_player_query_position (player, &pos))
		return;

	gst_element_seek (
		player->priv->pipeline,
		speed,
		GST_FORMAT_TIME,
		GST_SEEK_FLAG_SKIP,
		GST_SEEK_TYPE_SET,
		pos,
		GST_SEEK_TYPE_NONE,
		-1);

	pt_player_play (player);
}

gchar*
pt_player_get_uri (PtPlayer *player)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), NULL);

	gchar *uri = NULL;
	GFile *file = NULL;

	file = pt_player_get_file (player);
	if (file)
		uri = g_file_get_uri (file);

	g_object_unref (file);

	return uri;
}

gchar*
pt_player_get_filename (PtPlayer *player)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), NULL);

	GError	    *error = NULL;
	const gchar *filename = NULL;
	GFile       *file = NULL;
	GFileInfo   *info = NULL;
	gchar	    *result;

	file = pt_player_get_file (player);

	if (file)
		info = g_file_query_info (
				file,
				G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				G_FILE_QUERY_INFO_NONE,
				NULL,
				&error);
	else
		return NULL;

	if (error) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		g_object_unref (file);
		return NULL;
	}		
	
	filename = g_file_info_get_attribute_string (
				info,
				G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
	
	if (filename)
		result = g_strdup (filename);
	else
		result = NULL;

	if (info)
		g_object_unref (info);
	if (file)
		g_object_unref (file);

	return result;
}

static gchar*
pt_player_get_time_string (PtPlayer *player,
			   gint64    time,
			   guint     digits)
{
	gchar *result;
	gint   h, m, s, ms, mod;

	ms = GST_TIME_AS_MSECONDS (time);
	h = ms / 3600000;
	mod = ms % 3600000;
	m = mod / 60000;
	ms = ms % 60000;
	s = ms / 1000;
	ms = ms % 1000;

	if (GST_TIME_AS_SECONDS (player->priv->dur) > 3600)
		result = g_strdup_printf ("%d:%02d:%02d", h, m, s);
	else
		result = g_strdup_printf ("%02d:%02d", m, s);

	switch (digits) {
	case (1):
		result = g_strdup_printf ("%s-%d", result, ms / 100);
		break;
	case (2):
		result = g_strdup_printf ("%s-%02d", result, ms / 10);
		break;
	case (3):
		result = g_strdup_printf ("%s-%03d", result, ms);
		break;
	default:
		break;
	}

	return result;	
}

gchar*
pt_player_get_current_time_string (PtPlayer *player,
				   guint     digits)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), NULL);
	g_return_val_if_fail (digits <= 3, NULL);

	gint64 time;

	if (!pt_player_query_position (player, &time))
		return NULL;

	return pt_player_get_time_string (player, time, digits);
}

gchar*
pt_player_get_duration_time_string (PtPlayer *player,
				    guint     digits)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), NULL);
	g_return_val_if_fail (digits <= 3, NULL);

	gint64 time;

	if (!pt_player_query_duration (player, &time))
		return NULL;

	return pt_player_get_time_string (player, time, digits);
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
	PtPlayer *player = (PtPlayer *) data;
	
	//g_debug ("Message: %s; sent by: %s", GST_MESSAGE_TYPE_NAME (msg), GST_MESSAGE_SRC_NAME (msg));

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_signal_emit_by_name (player, "end-of-stream");
		break;

	case GST_MESSAGE_ERROR: {
		gchar  *debug;
		GError *error;

		gst_message_parse_error (msg, &error, &debug);
		g_debug ("ERROR from element %s: %s", GST_OBJECT_NAME (msg->src), error->message);
		g_debug ("Debugging info: %s", (debug) ? debug : "none");
		g_free (debug);
		g_signal_emit_by_name (player, "error", error->message);
		g_error_free (error);
		pt_player_clear (player);

		break;
		}
	case GST_MESSAGE_DURATION_CHANGED:
		pt_player_query_duration (player, &player->priv->dur);
		g_signal_emit_by_name (player, "duration-changed");
		break;
	default:
		break;
	}

	return TRUE;
}

static void
pt_player_init (PtPlayer *player)
{
	player->priv = pt_player_get_instance_private (player);
}

/* basically unchanged from 
   http://gstreamer.freedesktop.org/data/doc/gstreamer/head/manual/html/section-components-decodebin.html */
static void
decodebin_newpad_cb (GstElement *decodebin,
		     GstPad     *pad,
		     gpointer    data)
{
	PtPlayer *player = (PtPlayer *) data;

	GstCaps *caps;
	GstStructure *str;
	GstPad *audiopad;

	/* only link once */
	audiopad = gst_element_get_static_pad (player->priv->audio, "sink");
	if (GST_PAD_IS_LINKED (audiopad)) {
		g_object_unref (audiopad);
		return;
	}

	/* check media type */
	caps = gst_pad_query_caps (pad, NULL);
	str = gst_caps_get_structure (caps, 0);
	if (!g_strrstr (gst_structure_get_name (str), "audio")) {
		gst_caps_unref (caps);
		gst_object_unref (audiopad);
		return;
	}
	gst_caps_unref (caps);

	/* link'n'play */
	gst_pad_link (pad, audiopad);

	g_object_unref (audiopad);
}

static GstElement *
pt_player_create_element (const gchar *type,
			  const gchar *name,
			  GError      **error)
{
	GstElement *element = gst_element_factory_make (type, name);

	if (!element) {
		g_set_error (error,
			     GST_CORE_ERROR,
			     GST_CORE_ERROR_MISSING_PLUGIN,
			     _("Failed to load a plugin of type %s."), type);
		g_warning ("Failed to create element %s of type %s", name, type);
	}

	return element;
}

static gboolean
pt_player_initable_init (GInitable     *initable,
			 GCancellable  *cancellable,
			 GError       **error)
{
	GError *gst_error = NULL;

	PtPlayer *player;
	player = PT_PLAYER (initable);

	/* Setup player
	
	   We use the scaletempo plugin from "good plugins" with decodebin:
	   filesrc ! decodebin ! audioconvert ! audioresample ! volume ! scaletempo ! audioconvert ! audioresample ! autoaudiosink */

	gst_init_check (NULL, NULL, &gst_error);
	if (gst_error) {
		g_propagate_error (error, gst_error);
		return FALSE;
	}

	/* Create gstreamer elements */

	player->priv->pipeline = NULL;
	player->priv->source   = NULL;
	player->priv->volume   = NULL;
	GstElement *decodebin  = NULL;
	GstElement *convert    = NULL;
	GstElement *resample   = NULL;
	GstElement *scaletempo = NULL;
	GstElement *convert2   = NULL;
	GstElement *resample2  = NULL;
	GstElement *sink       = NULL;

	player->priv->pipeline = gst_pipeline_new ("player");

	player->priv->source = gst_element_factory_make ("filesrc",       "file-source");
	decodebin	     = gst_element_factory_make ("decodebin",     "decoder");
	convert		     = gst_element_factory_make ("audioconvert",  "convert");
	resample	     = gst_element_factory_make ("audioresample", "resample");
	player->priv->volume = gst_element_factory_make ("volume",	  "volume");
	scaletempo	     = gst_element_factory_make ("scaletempo",    "tempo");
	convert2	     = gst_element_factory_make ("audioconvert",  "convert2");
	resample2	     = gst_element_factory_make ("audioresample", "resample2");
	sink		     = gst_element_factory_make ("autoaudiosink", "sink");

	/* checks */
	if (!player->priv->pipeline || !player->priv->source || !decodebin
				    || !convert              || !resample
				    || !player->priv->volume || !scaletempo
				    || !convert2             || !resample2
				    || !sink) {
		g_set_error (error,
			     GST_CORE_ERROR,
			     GST_CORE_ERROR_MISSING_PLUGIN,
			     _("Failed to load a plugin."));
		return FALSE;
	}

	/* Set up the pipeline */
	g_signal_connect (decodebin, "pad-added", G_CALLBACK (decodebin_newpad_cb), player);
	gst_bin_add_many (GST_BIN (player->priv->pipeline), player->priv->source, decodebin, NULL);
	gst_element_link (player->priv->source, decodebin);

	/* create audio output */
	player->priv->audio = gst_bin_new ("audiobin");

	gst_bin_add_many (GST_BIN (player->priv->audio), convert,
							 resample,
							 player->priv->volume,
							 scaletempo,
							 convert2,
							 resample2,
							 sink,
							 NULL);

	gst_element_link_many (convert,
			       resample,
			       player->priv->volume,
			       scaletempo,
			       convert2,
			       resample2,
			       sink,
			       NULL);
	
	/* create ghost pad for audiosink */
	GstPad *audiopad = gst_element_get_static_pad (convert, "sink");
	gst_element_add_pad (player->priv->audio, gst_ghost_pad_new ("sink", audiopad));
	gst_object_unref (GST_OBJECT (audiopad));
	gst_bin_add (GST_BIN (player->priv->pipeline), player->priv->audio);

	return TRUE;
}

static void
pt_player_dispose (GObject *object)
{
	PtPlayer *player;
	player = PT_PLAYER (object);

	if (player->priv->pipeline) {
		/* remember position */
		metadata_save_position (player);
		
		pt_player_clear (player);
		gst_object_unref (GST_OBJECT (player->priv->pipeline));
		player->priv->pipeline = NULL;
		if (player->priv->bus_watch_id > 0)
			g_source_remove (player->priv->bus_watch_id);
	}

	G_OBJECT_CLASS (pt_player_parent_class)->dispose (object);
}

static void
pt_player_set_property (GObject      *object,
			guint         property_id,
			const GValue *value,
			GParamSpec   *pspec)
{
	PtPlayer *player;
	player = PT_PLAYER (object);

	switch (property_id) {
	case PROP_SPEED:
		player->priv->speed = g_value_get_double (value);
		break;
	case PROP_VOLUME:
		g_object_set (G_OBJECT (player->priv->volume), "volume", g_value_get_double (value), NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_player_get_property (GObject    *object,
			guint       property_id,
			GValue     *value,
			GParamSpec *pspec)
{
	PtPlayer *player;
	player = PT_PLAYER (object);
	gdouble tmp;

	switch (property_id) {
	case PROP_SPEED:
		g_value_set_double (value, player->priv->speed);
		break;
	case PROP_VOLUME:
		g_object_get (G_OBJECT (player->priv->volume), "volume", &tmp, NULL);
		g_value_set_double (value, tmp);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_player_class_init (PtPlayerClass *klass)
{
	G_OBJECT_CLASS (klass)->set_property = pt_player_set_property;
	G_OBJECT_CLASS (klass)->get_property = pt_player_get_property;
	G_OBJECT_CLASS (klass)->dispose = pt_player_dispose;

	g_signal_new ("duration-changed",
		      G_TYPE_OBJECT,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      0);

	g_signal_new ("end-of-stream",
		      G_TYPE_OBJECT,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      0);

	g_signal_new ("error",
		      G_TYPE_OBJECT,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE,
		      1, G_TYPE_STRING);

	obj_properties[PROP_SPEED] =
	g_param_spec_double (
			"speed",
			"Speed of playback",
			"1 is normal speed",
			0.1,	/* minimum */
			2.0,	/* maximum */
			1.0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	obj_properties[PROP_VOLUME] =
	g_param_spec_double (
			"volume",
			"Volume of playback",
			"Volume from 0 to 1",
			0.0,	/* minimum */
			1.0,	/* maximum */
			1.0,
			G_PARAM_READWRITE);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

static void
pt_player_initable_iface_init (GInitableIface *iface)
{
	iface->init = pt_player_initable_init;
}

PtPlayer *
pt_player_new (gdouble   speed,
	       GError  **error)
{
	return g_initable_new (PT_PLAYER_TYPE,
			NULL,	/* cancellable */
			error,
			"speed", speed,
			NULL);
}
