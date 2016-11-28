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
#include <stdio.h>	/* sscanf */
#include <gio/gio.h>
#define GETTEXT_PACKAGE PACKAGE
#include <glib/gi18n-lib.h>
#include <gst/gst.h>
#include "pt-waveloader.h"
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

	gint64      segstart;
	gint64      segend;

	GCancellable *c;
	gboolean      opening;

	PtWaveloader *wl;
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
static void remove_message_bus (PtPlayer *player);

G_DEFINE_TYPE_WITH_CODE (PtPlayer, pt_player, G_TYPE_OBJECT,
			 G_ADD_PRIVATE (PtPlayer)
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						pt_player_initable_iface_init))


/**
 * SECTION: pt-player
 * @short_description: The GStreamer backend for Parlatype.
 * @stability: Unstable
 * @include: parlatype/pt-player.h
 *
 * PtPlayer is the GStreamer backend for Parlatype. Construct it with #pt_player_new().
 * Then you have to open a file, either with pt_player_open_uri_async() or
 * pt_player_open_uri(), the blocking version.
 *
 * The internal time unit in PtPlayer are milliseconds and for scale widgets there
 * is a scale from 0 to 1000. Use it to jump to a position or to update your widget.
 *
 * While playing PtPlayer emits these signals:
 * - end-of-stream: End of file reached, in the GUI you might want to jump
 *    		       to the beginning, reset play button etc.
 * - error: A fatal error occured, the player is reset. There's an error message.
 *
 * PtPlayer has two properties:
 * - speed: is a double from 0.5 to 1.5. 1.0 is normal playback, < 1.0 is slower,
 *     > 1.0 is faster. Changing the "speed" property doesn't change playback though.
 *     Use the method instead.
 * - Volume is a double from 0 to 1. It can be set via the method or setting
      the "volume" property.
 */



/* -------------------------- static helpers -------------------------------- */

static gboolean
pt_player_query_position (PtPlayer *player,
			  gpointer  position)
{
	gboolean result;
	result = gst_element_query_position (player->priv->pipeline, GST_FORMAT_TIME, position);
	return result;
}

static void
pt_player_clear (PtPlayer *player)
{
	remove_message_bus (player);
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
	/* Saves current position in milliseconds as metadata to file */

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
	g_snprintf (value, sizeof (value), "%" G_GINT64_FORMAT, pos);

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
	/* Queries position stored in metadata from file.
	   Sets position to that value or to 0 */

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

static void
remove_message_bus (PtPlayer *player)
{
	if (player->priv->bus_watch_id > 0) {
		g_source_remove (player->priv->bus_watch_id);
		player->priv->bus_watch_id = 0;
	}
}

static void
add_message_bus (GTask *task)
{
	GstBus *bus;
	PtPlayer *player = g_task_get_source_object (task);

	remove_message_bus (player);
	bus = gst_pipeline_get_bus (GST_PIPELINE (player->priv->pipeline));
	player->priv->bus_watch_id = gst_bus_add_watch (bus, bus_call, task);
	gst_object_unref (bus);
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
	GTask	 *task = (GTask *) data;
	PtPlayer *player = g_task_get_source_object (task);

	/*g_debug ("Message: %s; sent by: %s",
			GST_MESSAGE_TYPE_NAME (msg),
			GST_MESSAGE_SRC_NAME (msg));*/

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_SEGMENT_DONE:
		/* From GStreamer documentation:
		   When performing a segment seek: after the playback of the segment completes,
		   no EOS will be emitted by the element that performed the seek, but a
		   GST_MESSAGE_SEGMENT_DONE message will be posted on the bus by the element. */

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

		if (player->priv->opening) {
			g_task_return_error (task, error);
		} else {
			g_signal_emit_by_name (player, "error", error);
			g_error_free (error);
		}

		pt_player_clear (player);
		break;
		}

	case GST_MESSAGE_ASYNC_DONE:
		if (player->priv->opening) {
			metadata_get_position (player);
			player->priv->opening = FALSE;
			g_task_return_boolean (task, TRUE);
		}
		break;

	default:
		break;
	}

	return TRUE;
}


/* -------------------------- opening files --------------------------------- */


static void
propagate_progress_cb (PtWaveloader *wl,
		       gdouble	     progress,
		       PtPlayer     *player)
{
	g_signal_emit_by_name (player, "load-progress", progress);
}

static void
load_cb (PtWaveloader *wl,
	 GAsyncResult *res,
	 gpointer     *data)
{
	GTask	 *task = (GTask *) data;
	PtPlayer *player = g_task_get_source_object (task);
	GError	 *error = NULL;

	if (pt_waveloader_load_finish (wl, res, &error)) {
		player->priv->dur = player->priv->segend = pt_waveloader_get_duration (player->priv->wl);
		player->priv->segstart = 0;

		/* setup message handler */
		add_message_bus (task);

		pt_player_pause (player);

	} else {
                g_clear_object (&wl);
		g_task_return_error (task, error);
	}
}

/**
 * pt_player_open_uri_finish:
 * @player: a #PtPlayer
 * @result: the #GAsyncResult passed to your #GAsyncReadyCallback
 * @error: a pointer to a NULL #GError, or NULL
 *
 * Gives the result of the async opening operation. A cancelled operation results
 * in an error, too.
 *
 * Return value: TRUE if successful, or FALSE with error set
 */
gboolean
pt_player_open_uri_finish (PtPlayer	 *player,
			   GAsyncResult  *result,
			   GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (result, player), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * pt_player_open_uri_async:
 * @player: a #PtPlayer
 * @uri: the URI of the file
 * @callback: a #GAsyncReadyCallback to call when the operation is complete
 * @user_data: user_data for callback
 *
 * Opens a local audio file for playback. It doesn't work with videos or streams.
 * Only one file can be open at a time, playlists are not supported by the
 * backend. Opening a new file will close the previous one.
 *
 * When closing a file or on object destruction PtPlayer tries to write the
 * last position into the file's metadata. On opening a file it reads the
 * metadata and jumps to the last known position if found.
 *
 * The player is set to the paused state and ready for playback. To start
 * playback use @pt_player_play().
 *
 * This is an asynchronous operation, to get the result call
 * pt_player_open_uri_finish() in your callback. For the blocking version see
 * pt_player_open_uri().
 *
 * While loading the file there is a #PtPlayer::load-progress signal emitted which stops before
 * reaching 100%. Don't use it to determine whether the operation is finished.
 */
void
pt_player_open_uri_async (PtPlayer	      *player,
			  gchar		      *uri,
			  GAsyncReadyCallback  callback,
			  gpointer	       user_data)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	g_return_if_fail (uri != NULL);

	GTask  *task;
	GFile  *file;
	gchar  *location = NULL;

	task = g_task_new (player, NULL, callback, user_data);
	player->priv->opening = TRUE;

	/* Change uri to location */
	file = g_file_new_for_uri (uri);
	location = g_file_get_path (file);
	
	if (!location) {
		g_task_return_new_error (
				task,
				GST_RESOURCE_ERROR,
				GST_RESOURCE_ERROR_NOT_FOUND,
				/* Translators: %s is a detailed error message. */
				_("URI not valid: %s"), uri);
		g_object_unref (file);
		g_object_unref (task);
		return;
	}

	if (!g_file_query_exists (file, NULL)) {
		g_task_return_new_error (
				task,
				GST_RESOURCE_ERROR,
				GST_RESOURCE_ERROR_NOT_FOUND,
				/* Translators: %s is a detailed error message. */
				_("File not found: %s"), location);
		g_object_unref (file);
		g_free (location);
		g_object_unref (task);
		return;
	}

	/* If we had an open file before, remember its position */
	metadata_save_position (player);

	/* Reset any open streams */
	pt_player_clear (player);
	player->priv->dur = -1;

	g_object_set (G_OBJECT (player->priv->source), "location", location, NULL);
	g_object_unref (file);
	g_free (location);

	player->priv->wl = pt_waveloader_new (uri);
	g_signal_connect (player->priv->wl,
			 "progress",
			 G_CALLBACK (propagate_progress_cb),
			 player);

	player->priv->c = g_cancellable_new ();
	pt_waveloader_load_async (player->priv->wl,
				  player->priv->c,
				  (GAsyncReadyCallback) load_cb,
				  task);
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

/**
 * pt_player_open_uri:
 * @player: a #PtPlayer
 * @uri: the URI of the file
 * @error: (allow-none): return location for an error, or NULL
 *
 * Opens a local audio file for playback. It doesn't work with videos or streams.
 * Only one file can be open at a time, playlists are not supported by the
 * backend. Opening a new file will close the previous one.
 *
 * When closing a file or on object destruction PtPlayer tries to write the
 * last position into the file's metadata. On opening a file it reads the
 * metadata and jumps to the last known position if found.
 *
 * The player is set to the paused state and ready for playback. To start
 * playback use @pt_player_play().
 *
 * This operation blocks until it is finished. It returns TRUE on success or
 * FALSE and an error. For the asynchronous version see
 * pt_player_open_uri_async().
 *
 * While loading the file there is a #PtPlayer::load-progress signal emitted.
 * However, it doesn't emit 100%, the operation is finished when TRUE is returned.
 *
 * Return value: TRUE if successful, or FALSE with error set
 */
gboolean
pt_player_open_uri (PtPlayer *player,
		    gchar    *uri,
		    GError  **error)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	gboolean      result;
	SyncData      data;
	GMainContext *context;

	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	data.loop = g_main_loop_new (context, FALSE);
	data.res = NULL;

	pt_player_open_uri_async (player, uri, (GAsyncReadyCallback) quit_loop_cb, &data);
	g_main_loop_run (data.loop);

	result = pt_player_open_uri_finish (player, data.res, error);

	return result;
}

/**
 * pt_player_cancel:
 * @player: a #PtPlayer
 *
 * Cancels the file opening operation, which triggers an error message.
 */
void
pt_player_cancel (PtPlayer *player)
{
	g_cancellable_cancel (player->priv->c);
}


/* ------------------------- Basic controls --------------------------------- */

/**
 * pt_player_pause:
 * @player: a #PtPlayer
 *
 * Sets the player to the paused state, meaning it stops playback and doesn't
 * change position. To resume playback use @pt_player_play().
 */
void
pt_player_pause (PtPlayer *player)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gst_element_set_state (player->priv->pipeline, GST_STATE_PAUSED);
}

/**
 * pt_player_play:
 * @player: a #PtPlayer
 *
 * Starts playback at the defined speed until it reaches the end of stream (or
 * the end of the selection)..
 */
void
pt_player_play (PtPlayer *player)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gst_element_set_state (player->priv->pipeline, GST_STATE_PLAYING);
}

/**
 * pt_player_set_selection:
 * @player: a #PtPlayer
 * @start: selection start time in milliseconds
 * @end: selection end time in milliseconds
 *
 * Set a selection. Sets the cursor to the start position. Playing will end at
 * the stop position and it's not possible to jump out of the selection until
 * it is cleared with #pt_player_clear_selection.
 */
void
pt_player_set_selection (PtPlayer *player,
		         gint64    start,
		         gint64    end)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	g_return_if_fail (start < end);

	player->priv->segstart = GST_MSECOND * start;
	player->priv->segend = GST_MSECOND * end;

	gst_element_seek (
		player->priv->pipeline,
		player->priv->speed,
		GST_FORMAT_TIME,
		GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
		GST_SEEK_TYPE_SET,
		player->priv->segstart,
		GST_SEEK_TYPE_SET,
		player->priv->segend);
}

/**
 * pt_player_clear_selection:
 * @player: a #PtPlayer
 *
 * Clear and reset any selection.
 */
void
pt_player_clear_selection (PtPlayer *player)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gint64 pos;

	if (!pt_player_query_position (player, &pos))
		return;

	player->priv->segstart = 0;
	player->priv->segend = player->priv->dur;

	gst_element_seek (
		player->priv->pipeline,
		player->priv->speed,
		GST_FORMAT_TIME,
		GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
		GST_SEEK_TYPE_SET,
		pos,
		GST_SEEK_TYPE_SET,
		-1);
}

/**
 * pt_player_rewind:
 * @player: a #PtPlayer
 * @speed: the speed
 *
 * Rewinds at the given speed.
 */
void
pt_player_rewind (PtPlayer *player,
		  gdouble   speed)
{
	/* FIXME Doesn't work at all!!! */
	/* Maybe because of: https://bugzilla.gnome.org/show_bug.cgi?id=757033 */

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

/**
 * pt_player_fast_forward:
 * @player: a #PtPlayer
 * @speed: the speed
 *
 * Play fast forward at the given speed.
 */
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

/* -------------------- Positioning, speed, volume -------------------------- */

/**
 * pt_player_jump_relative:
 * @player: a #PtPlayer
 * @milliseconds: time in milliseconds to jump
 *
 * Skips @milliseconds in stream. A positive value means jumping ahead. If the
 * resulting position would be beyond the end of stream (or selection), it goes
 * to the end of stream (or selection). A negative value means jumping back.
 * If the resulting position would be negative (or before the selection), it
 * jumps to position 0:00 (or to the start of the selection).
 */
void
pt_player_jump_relative (PtPlayer *player,
			 gint      milliseconds)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	if (milliseconds == 0)
		return;

	gint64 pos, new;

	if (!pt_player_query_position (player, &pos))
		return;
	
	new = pos + GST_MSECOND * (gint64) milliseconds;
	g_debug ("jump relative:\n"
			"dur = %" G_GINT64_FORMAT "\n"
			"new = %" G_GINT64_FORMAT,
			player->priv->dur, new);

	if (new > player->priv->segend)
		new = player->priv->segend;

	if (new < player->priv->segstart)
		new = player->priv->segstart;

	pt_player_seek (player, new);
}

/**
 * pt_player_jump_to_position:
 * @player: a #PtPlayer
 * @milliseconds: position in milliseconds
 *
 * Jumps to a given position in stream. The position is given in @milliseconds
 * starting from position 0:00. A position beyond the duration of stream (or
 * outside the selection) is ignored.
 */
void
pt_player_jump_to_position (PtPlayer *player,
			    gint      milliseconds)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	gint64 pos;

	pos = GST_MSECOND * (gint64) milliseconds;

	if (pos > player->priv->segend || pos < player->priv->segstart) {
		g_debug ("jump to position failed\n"
				"dur = %" G_GINT64_FORMAT "\n"
				"pos = %" G_GINT64_FORMAT,
				player->priv->dur, pos);
		return;
	}

	pt_player_seek (player, pos);
}

/**
 * pt_player_jump_to_permille:
 * @player: a #PtPlayer
 * @permille: scale position between 0 and 1000
 *
 * This is used for scale widgets. Start of stream is at 0, end of stream is
 * at 1000. This will jump to the given position. If your widget uses a different
 * scale, it's up to you to convert it to 1/1000. Values beyond 1000 are not
 * allowed, values outside the selection are ignored.
 */
void
pt_player_jump_to_permille (PtPlayer *player,
			    guint     permille)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	g_return_if_fail (permille <= 1000);

	gint64 new;

	new = player->priv->dur * (gint64) permille / 1000;
	if (new > player->priv->segend || new < player->priv->segstart)
		return;

	pt_player_seek (player, new);
}

/**
 * pt_player_get_permille:
 * @player: a #PtPlayer
 *
 * This is used for scale widgets. If the scale has to synchronize with the
 * current position in stream, this gives the position on a scale between 0 and
 * 1000.
 *
 * Failure in querying the position returns -1.
 *
 * Return value: a scale position between 0 and 1000 or -1 on failure
 */
gint
pt_player_get_permille (PtPlayer *player)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), -1);

	gint64 pos;

	if (!pt_player_query_position (player, &pos))
		return -1;

	return (gfloat) pos / (gfloat) player->priv->dur * 1000;
}

/**
 * pt_player_set_speed:
 * @player: a #PtPlayer
 * @speed: speed
 *
 * Sets the speed of playback in the paused state as well as during playback.
 * Normal speed is 1.0, everything above that is faster, everything below slower.
 * A speed of 0 is not allowed, use pt_player_pause() instead.
 * Recommended speed is starting from 0.5 as quality is rather poor below that.
 * Parlatype doesn't change the pitch during slower or faster playback.
 *
 * Note: If you want to change the speed during playback, you have to use this
 * method. Changing the "speed" property of PtPlayer, will take effect only
 * later.
 */
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

/**
 * pt_player_set_volume:
 * @player: a #PtPlayer
 * @volume: volume
 *
 * Sets the volume on a scale between 0 and 1. Instead of using this method
 * you could set the "volume" property.
 */
void
pt_player_set_volume (PtPlayer *player,
		      gdouble   volume)
{
	g_return_if_fail (PT_IS_PLAYER (player));
	g_return_if_fail (volume >= 0 && volume <= 1);

	g_object_set (G_OBJECT (player->priv->volume), "volume", volume, NULL);
}

/**
 * pt_player_mute_volume:
 * @player: a #PtPlayer
 * @mute: a gboolean
 *
 * Mute the player (with TRUE) or set it back to normal volume (with FALSE).
 * This remembers the volume level, so you don't have to keep track of the old value.
 */
void
pt_player_mute_volume (PtPlayer *player,
		       gboolean  mute)
{
	g_return_if_fail (PT_IS_PLAYER (player));

	g_object_set (G_OBJECT (player->priv->volume), "mute", mute, NULL);
}

/**
 * pt_player_get_position:
 * @player: a #PtPlayer
 *
 * Returns the current position in stream.
 *
 * Return value: position in milliseconds or -1 on failure
 */
gint
pt_player_get_position (PtPlayer *player)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), -1);

	gint64 time;

	if (!pt_player_query_position (player, &time))
		return -1;

	return GST_TIME_AS_MSECONDS (time);
}

/**
 * pt_player_get_duration:
 * @player: a #PtPlayer
 *
 * Returns the duration of stream.
 *
 * Return value: duration in milliseconds
 */
gint
pt_player_get_duration (PtPlayer *player)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), -1);

	return GST_TIME_AS_MSECONDS (player->priv->dur);
}


/* ------------------------- Waveform stuff --------------------------------- */

/**
 * pt_player_wave_pos:
 * @player: a #PtPlayer
 *
 * Translates the current position in stream into a sample number that can be
 * used by a wave viewer widget.
 *
 * Return value: index of the data array
 */
gint64
pt_player_wave_pos (PtPlayer *player)
{
	gint64 pos;

	if (!pt_player_query_position (player, &pos))
		return 0;

	return GST_TIME_AS_MSECONDS (pos);
}

/**
 * pt_player_get_data:
 * @player: a #PtPlayer
 *
 * Returns all data for visual representation.
 *
 * Return value: (transfer full): the #PtWavedata
 */
PtWavedata*
pt_player_get_data (PtPlayer *player)
{
	return pt_waveloader_get_data (player->priv->wl);
}


/* --------------------- File utilities ------------------------------------- */

/**
 * pt_player_get_uri:
 * @player: a #PtPlayer
 *
 * Returns the URI of the currently open file or NULL if it can't be determined.
 *
 * Return value: (transfer full): the uri
 */
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

/**
 * pt_player_get_filename:
 * @player: a #PtPlayer
 *
 * Returns the display name of the currently open file or NULL if it can't be determined.
 *
 * Return value: (transfer full): the file name
 */
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

/* --------------------- Time strings and timestamps ------------------------ */

/**
 * pt_player_get_time_string:
 * @time: time in milliseconds to converse
 * @duration: duration of stream in milliseconds (max time)
 * @precision: a #PtPrecisionType
 *
 * Returns the given time as a string for display to the user. Format type is
 * determined by @duration, e.g. if duration is long format, it returns a string
 * in long format, too.
 *
 * Return value: (transfer full): the time string
 */
gchar*
pt_player_get_time_string (gint  time,
			   gint  duration,
			   PtPrecisionType precision)
{
	/* Don't assert time <= duration because duration is not exact */

	g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

	gchar *result;
	gint   h, m, s, ms, mod;

	h = time / 3600000;
	mod = time % 3600000;
	m = mod / 60000;
	ms = time % 60000;
	s = ms / 1000;
	ms = ms % 1000;

	/* Short or long format depends on total duration */
	if ((duration / 1000) >= 3600) {
		if (precision == PT_PRECISION_SECOND) {
		/* Translators: This is a time format, like "2:05:30" for 2
		   hours, 5 minutes, and 30 seconds. You may change ":" to
		   the separator that your locale uses or use "%Id" instead
		   of "%d" if your locale uses localized digits. */
			result = g_strdup_printf (C_("long time format", "%d:%02d:%02d"), h, m, s);
		} else if (precision == PT_PRECISION_SECOND_10TH) {
		/* Translators: This is a time format, like "2:05:30.1" for 2
		   hours, 5 minutes, 30 seconds, and 1 tenthsecond. You may
		   change ":" or "." to the separator that your locale uses or
		   use "%Id" instead of "%d" if your locale uses localized digits. */
			result = g_strdup_printf (C_("long time format, 1 digit", "%d:%02d:%02d.%d"), h, m, s, ms / 100);
		} else if (precision == PT_PRECISION_SECOND_100TH) {
		/* Translators: This is a time format, like "2:05:30.12" for 2
		   hours, 5 minutes, 30 seconds, and 12 hundrethseconds. You may
		   change ":" or "." to the separator that your locale uses or
		   use "%Id" instead of "%d" if your locale uses localized digits. */
			result = g_strdup_printf (C_("long time format, 2 digits", "%d:%02d:%02d.%02d"), h, m, s, ms / 10);
		}
	} else {
		if ((duration / 1000) >= 600) {
			if (precision == PT_PRECISION_SECOND) {
			/* Translators: This is a time format, like "05:30" for 
			   5 minutes, and 30 seconds. You may change ":" to
			   the separator that your locale uses or use "%I02d" instead
			   of "%02d" if your locale uses localized digits. */
				result = g_strdup_printf (C_("short time format", "%02d:%02d"), m, s);
			} else if (precision == PT_PRECISION_SECOND_10TH) {
			/* Translators: This is a time format, like "05:30.1" for
			   5 minutes, 30 seconds, and 1 tenthsecond. You may change
			   ":" or "." to the separator that your locale uses or
			   use "%Id" instead of "%d" if your locale uses localized digits. */
				result = g_strdup_printf (C_("short time format, 1 digit", "%02d:%02d.%d"), m, s, ms / 100);
			} else if (precision == PT_PRECISION_SECOND_100TH) {
			/* Translators: This is a time format, like "05:30.12" for
			   5 minutes, 30 seconds, and 12 hundrethseconds. You may change
			   ":" or "." to the separator that your locale uses or
			   use "%Id" instead of "%d" if your locale uses localized digits. */
				result = g_strdup_printf (C_("short time format, 2 digits", "%02d:%02d.%02d"), m, s, ms / 10);
			}
		} else {
			if (precision == PT_PRECISION_SECOND) {
			/* Translators: This is a time format, like "5:30" for 
			   5 minutes, and 30 seconds. You may change ":" to
			   the separator that your locale uses or use "%Id" instead
			   of "%d" if your locale uses localized digits. */
				result = g_strdup_printf (C_("shortest time format", "%d:%02d"), m, s);
			} else if (precision == PT_PRECISION_SECOND_10TH) {
			/* Translators: This is a time format, like "05:30.1" for
			   5 minutes, 30 seconds, and 1 tenthsecond. You may change
			   ":" or "." to the separator that your locale uses or
			   use "%Id" instead of "%d" if your locale uses localized digits. */
				result = g_strdup_printf (C_("shortest time format, 1 digit", "%d:%02d.%d"), m, s, ms / 100);
			} else if (precision == PT_PRECISION_SECOND_100TH) {
			/* Translators: This is a time format, like "05:30.12" for
			   5 minutes, 30 seconds, and 12 hundrethseconds. You may change
			   ":" or "." to the separator that your locale uses or
			   use "%Id" instead of "%d" if your locale uses localized digits. */
				result = g_strdup_printf (C_("shortest time format, 2 digits", "%d:%02d.%02d"), m, s, ms / 10);
			}
		}
	}

	return result;	
}

/**
 * pt_player_get_current_time_string:
 * @player: a #PtPlayer
 * @precision: a #PtPrecisionType
 *
 * Returns the current position of the stream as a string for display to the user.
 *
 * If the current position can not be determined, NULL is returned.
 *
 * Return value: (transfer full): the time string
 */
gchar*
pt_player_get_current_time_string (PtPlayer *player,
				   PtPrecisionType precision)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), NULL);
	g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

	gint64 time;

	if (!pt_player_query_position (player, &time))
		return NULL;

	return pt_player_get_time_string (
			GST_TIME_AS_MSECONDS (time),
			GST_TIME_AS_MSECONDS (player->priv->dur),
			precision);
}

/**
 * pt_player_get_duration_time_string:
 * @player: a #PtPlayer
 * @precision: a #PtPrecisionType
 *
 * Returns the duration of the stream as a string for display to the user.
 *
 * If the duration can not be determined, NULL is returned.
 *
 * Return value: (transfer full): the time string
 */
gchar*
pt_player_get_duration_time_string (PtPlayer *player,
				    PtPrecisionType precision)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), NULL);
	g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

	return pt_player_get_time_string (
			GST_TIME_AS_MSECONDS (player->priv->dur),
			GST_TIME_AS_MSECONDS (player->priv->dur),
			precision);
}

/**
 * pt_player_get_timestamp:
 * @player: a #PtPlayer
 *
 * Returns the current timestamp as a string. The format of timestamps can not be changed.
 *
 * If the current position can not be determined, NULL is returned.
 *
 * Return value: (transfer full): the timestamp
 */
gchar*
pt_player_get_timestamp (PtPlayer *player)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), NULL);

	gchar *tmp;
	gchar *timestamp;
	gint64 time;

	if (!pt_player_query_position (player, &time))
		return NULL;

	tmp = NULL;
	timestamp = NULL;

	tmp = pt_player_get_time_string (
			GST_TIME_AS_MSECONDS (time),
			GST_TIME_AS_MSECONDS (player->priv->dur),
			1);
	if (tmp) {
		timestamp = g_strdup_printf ("#%s#", tmp);
		g_free (tmp);
	}

	return timestamp;
}

static gint
pt_player_get_timestamp_position (PtPlayer *player,
				  gchar    *timestamp)
{
	gint      h, m, s, digit, args, result;
	gchar    *cmp = NULL;
	gchar   **split = NULL;

	if (g_str_has_prefix (timestamp, "#")) {
		split = g_strsplit (timestamp, "#", -1);
		if (split[1]) {
			cmp = g_strdup (split[1]);
		}
		if (split) {
			g_strfreev (split);
		}
		if (!cmp) {
			return -1;
		}
	} else {
		cmp = g_strdup (timestamp);
	}

	args = sscanf (cmp, C_("long time format, 1 digit", "%d:%02d:%02d.%01d"), &h, &m, &s, &digit);

	if (args != 4) {
		h = 0;
		args = sscanf (cmp, C_("shortest time format, 1 digit", "%d:%02d.%01d"), &m, &s, &digit);
		if (args != 3) {
			g_free (cmp);
			return -1;
		}
	}
	g_free (cmp);

	if (s > 59 || m > 59)
		return -1;

	result = ((h * 3600 + m * 60 + s) * 10 + digit ) * 100;

	if (GST_MSECOND * (gint64) result > player->priv->dur)
		return -1;

	return result;
}

/**
 * pt_player_string_is_timestamp:
 * @player: a #PtPlayer
 * @timestamp: the string to be checked
 *
 * Returns whether the given string is a valid timestamp.
 *
 * See also pt_player_goto_timestamp() if you want to go to the timestamp's
 * position immediately after.
 *
 * Return value: TRUE if the timestamp is valid, FALSE if not
 */
gboolean
pt_player_string_is_timestamp (PtPlayer *player,
			       gchar    *timestamp)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), FALSE);
	g_return_val_if_fail (timestamp != NULL, FALSE);

	return (pt_player_get_timestamp_position (player, timestamp) != -1);
}

/**
 * pt_player_goto_timestamp:
 * @player: a #PtPlayer
 * @timestamp: the timestamp to go to
 *
 * Goes to the position of the timestamp. Returns false, if it's not a
 * valid timestamp.
 *
 * Return value: TRUE on success, FALSE if the timestamp is not valid
 */
gboolean
pt_player_goto_timestamp (PtPlayer *player,
			  gchar    *timestamp)
{
	g_return_val_if_fail (PT_IS_PLAYER (player), FALSE);
	g_return_val_if_fail (timestamp != NULL, FALSE);

	gint pos;

	pos = pt_player_get_timestamp_position (player, timestamp);

	if (pos == -1)
		return FALSE;

	pt_player_jump_to_position (player, pos);
	return TRUE;
}

/* --------------------- Init and GObject management ------------------------ */

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
		
		gst_element_set_state (player->priv->pipeline, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (player->priv->pipeline));
		player->priv->pipeline = NULL;
		remove_message_bus (player);
	}

	g_clear_object (&player->priv->wl);

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

	/**
	* PtPlayer::load-progress:
	* @player: the player emitting the signal
	* @progress: the new progress state, ranging from 0.0 to 1.0
	*
	* Indicates progress on a scale from 0.0 to 1.0, however it does not
	* emit the value 0.0 nor 1.0. Wait for a TRUE player-state-changed
	* signal or an error signal to dismiss a gui element showing progress.
	*/
	g_signal_new ("load-progress",
		      PT_TYPE_PLAYER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__DOUBLE,
		      G_TYPE_NONE,
		      1, G_TYPE_DOUBLE);

	/**
	* PtPlayer::end-of-stream:
	* @player: the player emitting the signal
	*
	* The #PtPlayer::end-of-stream signal is emitted when the stream is at its end
	* or when the end of selection is reached.
	*/
	g_signal_new ("end-of-stream",
		      PT_TYPE_PLAYER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      0);

	/**
	* PtPlayer::error:
	* @player: the player emitting the signal
	* @error: a GError
	*
	* The #PtPlayer::error signal is emitted on errors opening the file or during
	* playback. It's a severe error and the player is always reset.
	*/
	g_signal_new ("error",
		      PT_TYPE_PLAYER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1, G_TYPE_ERROR);

	/**
	* PtPlayer:speed:
	*
	* The speed for playback.
	*/
	obj_properties[PROP_SPEED] =
	g_param_spec_double (
			"speed",
			"Speed of playback",
			"1 is normal speed",
			0.1,	/* minimum */
			2.0,	/* maximum */
			1.0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	/**
	* PtPlayer:volume:
	*
	* The volume for playback.
	*/
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

/**
 * pt_player_new:
 * @error: (allow-none): return location for an error, or NULL
 *
 * This is a failable constructor. It fails, if GStreamer doesn't init or a
 * plugin is missing. In this case NULL is returned, error is set.
 *
 * After use g_object_unref() it.
 *
 * Return value: (transfer full): a new pt_player
 */
PtPlayer *
pt_player_new (GError **error)
{
	return g_initable_new (PT_TYPE_PLAYER,
			NULL,	/* cancellable */
			error,
			NULL);
}
