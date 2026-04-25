/* Copyright 2016 Gabor Karsay <gabor.karsay@gmx.at>
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
 * Parts of this program are taken from or inspired by GStreamer’s gstplay.c.
 */

/**
 * PtPlayer:
 *
 * GStreamer backend for Parlatype.
 *
 * PtPlayer is the GStreamer backend for Parlatype. Construct it with #pt_player_new().
 * Then you have to open a file with pt_player_open_uri().
 *
 * The internal time unit in PtPlayer are milliseconds.
 *
 * The internal GStreamer pipeline looks like this, only GstAppSink is directly exposed:
 * ```
 * .----------------.  .---------------------------------.
 * | playbin3       |  | pt_audio_bin                    |
 * |                |  | .------.  .-------------------. |
 * | filter:        |  | | tee  |  | pt_audio_play_bin | |
 * | scaletempo     ----->      --->                   | |
 * '----------------'  | |      |  | sink: pulsesink   | |
 *                     | |      |  '-------------------' |
 *                     | |      |  .-------------------. |
 *                     | |      |  | pt_audio_asr_bin  | |
 *                     | |      --->                   | |
 *                     | |      |  | sink: gstappsink  | |
 *                     | '------'  '-------------------' |
 *                     '---------------------------------'
 * ```
 */

#define GETTEXT_PACKAGE GETTEXT_LIB

#include "config.h"

#include "pt-player.h"

#include "gst/gst-helpers.h"
#include "gst/gstptaudiobin.h"
#include "pt-config.h"
#include "pt-i18n.h"
#include "pt-media-info-private.h"
#include "pt-media-info.h"
#include "pt-position-manager.h"
#include "pt-text-view.h"
#include "pt-timestamp.h"
#include "pt-waveviewer.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gst/audio/streamvolume.h>

struct _PtPlayer
{
  GObject parent_instance;

  GstElement *play;
  GstElement *scaletempo;
  GstElement *audio_bin;
  guint       bus_watch_id;

  PtMediaInfo         *info;
  GstStreamCollection *collection;
  gchar               *stream_id;
  gulong               stream_notify_id;

  PtPositionManager *pos_mgr;

  GObject *asr_plugin;
  gboolean is_configured;

  PtStateType app_state;
  GstState    current_state;
  GstState    target_state;

  GMutex       lock;
  gboolean     seek_pending;
  GstClockTime last_seek_time;
  GSource     *seek_source;
  GstClockTime seek_position;

  gint64   dur;
  gdouble  speed;
  gdouble  volume;
  gboolean mute;
  gint     pause;
  gint     back;
  gint     forward;
  gboolean repeat_all;
  gboolean repeat_selection;

  gint64       segstart;
  GstClockTime segend;

  GCancellable *c;
  guint         vol_changed_id;
  guint         mute_changed_id;
  guint         uri_changed_id;

  PtPrecisionType timestamp_precision;
  gboolean        timestamp_fixed;
  gchar          *timestamp_left;
  gchar          *timestamp_right;
  gchar          *timestamp_sep;

  PtWaveviewer *wv;
  gboolean      set_follow_cursor;
};

enum
{
  PROP_STATE = 1,
  PROP_CURRENT_URI,
  PROP_SPEED,
  PROP_MUTE,
  PROP_VOLUME,
  PROP_TIMESTAMP_PRECISION,
  PROP_TIMESTAMP_FIXED,
  PROP_TIMESTAMP_DELIMITER,
  PROP_TIMESTAMP_FRACTION_SEP,
  PROP_REWIND_ON_PAUSE,
  PROP_BACK,
  PROP_FORWARD,
  PROP_REPEAT_ALL,
  PROP_REPEAT_SELECTION,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

#define ONE_HOUR 3600000
#define TEN_MINUTES 600000

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);

G_DEFINE_FINAL_TYPE (PtPlayer, pt_player, G_TYPE_OBJECT)

/* -------------------------- static helpers -------------------------------- */

static void
pt_player_clear (PtPlayer *self)
{
  g_clear_handle_id (&self->bus_watch_id, g_source_remove);
  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  gst_element_set_state (self->play, GST_STATE_NULL);
}

static void
remove_seek_source (PtPlayer *self)
{
  if (!self->seek_source)
    return;

  g_source_destroy (self->seek_source);
  g_source_unref (self->seek_source);
  self->seek_source = NULL;
}

static void
pt_player_seek_internal_locked (PtPlayer *self)
{
  gboolean             ret;
  GstClockTime         position;
  gdouble              speed;
  gint64               stop;
  GstStateChangeReturn state_ret;

  remove_seek_source (self);

  if (self->current_state < GST_STATE_PAUSED)
    return;

  if (self->current_state != GST_STATE_PAUSED)
    {
      g_mutex_unlock (&self->lock);
      state_ret = gst_element_set_state (self->play, GST_STATE_PAUSED);
      if (state_ret == GST_STATE_CHANGE_FAILURE)
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                            "MESSAGE", "Failed to seek");
          /* on error */
        }
      g_mutex_lock (&self->lock);
      return;
    }

  self->last_seek_time = gst_util_get_timestamp ();
  position = self->seek_position;
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->seek_pending = TRUE;
  speed = self->speed;
  stop = self->segend;
  g_mutex_unlock (&self->lock);

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Seek to position %" GST_TIME_FORMAT ", stop at %" GST_TIME_FORMAT,
                    GST_TIME_ARGS (position), GST_TIME_ARGS (stop));

  ret = gst_element_seek (
      self->play,
      speed,
      GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
      GST_SEEK_TYPE_SET,
      position,
      GST_SEEK_TYPE_SET,
      stop);

  if (!ret)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Failed to seek to %" GST_TIME_FORMAT,
                        GST_TIME_ARGS (position));
    }

  g_mutex_lock (&self->lock);
}

static gboolean
pt_player_seek_internal (gpointer user_data)
{
  PtPlayer *self = PT_PLAYER (user_data);

  g_mutex_lock (&self->lock);
  pt_player_seek_internal_locked (self);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static void
pt_player_seek (PtPlayer *self,
                gint64    position)
{
  g_mutex_lock (&self->lock);

  self->seek_position = position;

  if (!self->seek_source)
    {
      GstClockTime now = gst_util_get_timestamp ();
      if (!self->seek_pending || (now - self->last_seek_time > 250 * GST_MSECOND))
        {
          self->seek_source = g_idle_source_new ();
          g_source_set_callback (self->seek_source, (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Dispatching seek to position %" GST_TIME_FORMAT,
                            GST_TIME_ARGS (position));
          g_source_attach (self->seek_source, NULL);
        }
      else
        {
          guint delay = 250000 - (now - self->last_seek_time) / 1000;
          self->seek_source = g_timeout_source_new (delay);
          g_source_set_callback (self->seek_source, (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_source_set_callback (self->seek_source, (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Delaying seek to position %" GST_TIME_FORMAT "by %u microseconds",
                            GST_TIME_ARGS (position), delay);
          g_source_attach (self->seek_source, NULL);
        }
    }

  g_mutex_unlock (&self->lock);
}

static GFile *
pt_player_get_file (PtPlayer *self)
{
  gchar *uri = NULL;
  GFile *result = NULL;

  g_object_get (G_OBJECT (self->play), "current-uri", &uri, NULL);

  if (uri)
    {
      result = g_file_new_for_uri (uri);
      g_free (uri);
    }

  return result;
}

static void
metadata_save_position (PtPlayer *self)
{
  GFile *file = NULL;
  gint64 pos;

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &pos))
    return;

  file = pt_player_get_file (self);
  if (!file)
    return;

  pos = pos / GST_MSECOND;

  pt_position_manager_save (self->pos_mgr, file, pos);
  g_object_unref (file);
}

static void
metadata_goto_position (PtPlayer *self)
{
  GFile *file = NULL;
  gint64 pos;

  file = pt_player_get_file (self);
  if (!file)
    return;

  pos = pt_position_manager_load (self->pos_mgr, file);

  pt_player_jump_to_position (self, pos);
  g_object_unref (file);
}

static const gchar *
pt_player_get_state_name (PtStateType state)
{
  switch (state)
    {
    case PT_STATE_STOPPED:
      return "stopped";
    case PT_STATE_PAUSED:
      return "paused";
    case PT_STATE_PLAYING:
      return "playing";
    case PT_STATE_TRANSCRIBING:
      return "transcribing";
    case PT_STATE_INVALID:
      return NULL;
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static void
change_app_state (PtPlayer   *self,
                  PtStateType state)
{
  if (state == self->app_state)
    return;

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Changing app state from %s to %s",
                    pt_player_get_state_name (self->app_state),
                    pt_player_get_state_name (state));

  self->app_state = state;

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_STATE]);
}

static void
stream_notify_cb (GstStreamCollection *collection,
                  GstStream           *stream,
                  GParamSpec          *pspec,
                  gpointer             user_data)
{
  PtPlayer    *self = (PtPlayer *) user_data;
  const gchar *stream_id;
  gboolean     our_stream;
  gboolean     tags_changed;

  tags_changed = (G_PARAM_SPEC_VALUE_TYPE (pspec) == GST_TYPE_TAG_LIST);
  stream_id = gst_stream_get_stream_id (stream);

  g_mutex_lock (&self->lock);
  our_stream = (g_strcmp0 (self->stream_id, stream_id) == 0);

  if (our_stream && tags_changed)
    _pt_media_info_update_tags (self->info, gst_stream_get_tags (stream));
  g_mutex_unlock (&self->lock);
}

static void
update_stream_collection (PtPlayer            *self,
                          GstStreamCollection *collection)
{
  if (self->collection && self->collection == collection)
    return;

  g_clear_signal_handler (&self->stream_notify_id, self->collection);

  gst_object_replace ((GstObject **) &self->collection,
                      (GstObject *) collection);

  self->stream_notify_id = g_signal_connect (self->collection,
                                             "stream-notify",
                                             G_CALLBACK (stream_notify_cb),
                                             self);
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  PtPlayer *self = (PtPlayer *) data;
  gint64    pos, stop;

  switch (GST_MESSAGE_TYPE (msg))
    {
    case GST_MESSAGE_EOS:
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE", "EOS");
      /* Sometimes the current position is not exactly at the end which looks like
       * playback stopped too early or makes the cursor disappear.*/
      gst_element_query_position (self->play, GST_FORMAT_TIME, &pos);
      stop = GST_CLOCK_TIME_IS_VALID (self->segend) ? (gint64) self->segend : self->dur;
      if (pos != stop)
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Correcting EOS position: %" G_GINT64_FORMAT " ms",
                            GST_TIME_AS_MSECONDS (stop - pos));
          pt_player_seek (self, stop);
        }
      g_signal_emit_by_name (self, "end-of-stream");
      break;

    case GST_MESSAGE_DURATION_CHANGED:
      {
        gint64 dur;
        gst_element_query_duration (self->play, GST_FORMAT_TIME, &dur);
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE",
                          "New duration: %" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
        self->dur = dur;
        if (GST_CLOCK_TIME_IS_VALID (self->segend) &&
            self->segend > (guint64) dur)
          {
            self->segend = dur;
            gst_element_query_position (self->play, GST_FORMAT_TIME, &pos);
            pt_player_seek (self, pos);
          }
        break;
      }
    case GST_MESSAGE_STATE_CHANGED:
      {
        if (GST_MESSAGE_SRC (msg) != GST_OBJECT (self->play))
          break;
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          "MESSAGE", "state changed, old state: %s", gst_element_state_get_name (old_state));
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          "MESSAGE", "state changed, new state: %s", gst_element_state_get_name (new_state));
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          "MESSAGE", "state changed, pending state: %s", gst_element_state_get_name (pending_state));
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          "MESSAGE", "state changed, seek pending: %s", self->seek_pending ? "yes" : "no");

        self->current_state = new_state;

        if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED && pending_state == GST_STATE_VOID_PENDING)
          {
            g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                              "MESSAGE", "Initial PAUSED - pre-rolled");
          }

        if (new_state == GST_STATE_PAUSED && pending_state == GST_STATE_VOID_PENDING)
          {
            g_mutex_lock (&self->lock);
            if (self->seek_pending)
              {
                self->seek_pending = FALSE;
                if (self->seek_source)
                  {
                    g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE",
                                      "Seek finished but new seek is pending");
                    pt_player_seek_internal_locked (self);
                  }
                else
                  {
                    g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE",
                                      "Seek finished");
                    g_signal_emit_by_name (self, "seek-done");
                  }
              }
            if (self->seek_position != GST_CLOCK_TIME_NONE)
              {
                g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE",
                                  "Seeking now that we reached PAUSED state");
                pt_player_seek_internal_locked (self);
                g_mutex_unlock (&self->lock);
              }
            else if (!self->seek_pending)
              {
                g_mutex_unlock (&self->lock);
                if (self->target_state >= GST_STATE_PLAYING)
                  {
                    GstStateChangeReturn state_ret;
                    state_ret = gst_element_set_state (self->play, GST_STATE_PLAYING);
                    if (state_ret == GST_STATE_CHANGE_FAILURE)
                      {
                        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                                          "MESSAGE", "Failed to play");
                      }
                  }
                else
                  {
                    change_app_state (self, PT_STATE_PAUSED);
                  }
              }
            else
              {
                g_mutex_unlock (&self->lock);
              }
          }

        if (new_state == GST_STATE_PLAYING && pending_state == GST_STATE_VOID_PENDING)
          {
            PtStateType state = PT_STATE_PLAYING;
            if (pt_player_get_mode (self) == PT_MODE_ASR)
              {
                state = PT_STATE_TRANSCRIBING;
              }
            change_app_state (self, state);
          }
        if (new_state == GST_STATE_READY &&
            old_state > GST_STATE_READY)
          {
            change_app_state (self, PT_STATE_STOPPED);
          }
        break;
      }
    case GST_MESSAGE_ERROR:
      {
        gchar  *debug;
        GError *error;

        gst_message_parse_error (msg, &error, &debug);

        /* Error is returned. Log the message here at level DEBUG,
           as higher levels will abort tests. */

        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                          "MESSAGE", "Error from element %s: %s", GST_OBJECT_NAME (msg->src), error->message);
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                          "MESSAGE", "Debugging info: %s", (debug) ? debug : "none");
        g_free (debug);

        gst_debug_bin_to_dot_file_with_ts (GST_BIN (self->play), GST_DEBUG_GRAPH_SHOW_ALL, "player-error");

        g_signal_emit_by_name (self, "error", error);
        g_error_free (error);
        pt_player_clear (self);
        break;
      }

    case GST_MESSAGE_STREAMS_SELECTED:;
      GstStreamCollection *collection = NULL;
      gboolean             found_stream = FALSE;
      guint                i, len;

      gst_message_parse_streams_selected (msg, &collection);
      if (collection == NULL)
        break;

      g_mutex_lock (&self->lock);
      update_stream_collection (self, collection);
      g_mutex_unlock (&self->lock);

      gst_object_unref (collection);

      len = gst_message_streams_selected_get_size (msg);
      for (i = 0; i < len; i++)
        {
          GstStream    *stream;
          GstStreamType stream_type;
          stream = gst_message_streams_selected_get_stream (msg, i);
          stream_type = gst_stream_get_stream_type (stream);
          if ((stream_type & GST_STREAM_TYPE_AUDIO) == 0)
            continue;

          if (G_UNLIKELY (found_stream))
            {
              GST_FIXME_OBJECT (self, "Multiple audio streams selected, first one chosen");
              continue;
            }

          found_stream = TRUE;
          g_mutex_lock (&self->lock);
          g_free (self->stream_id);
          self->stream_id = g_strdup (gst_stream_get_stream_id (stream));
          _pt_media_info_update_caps (self->info, gst_stream_get_caps (stream));
          _pt_media_info_update_tags (self->info, gst_stream_get_tags (stream));
          g_mutex_unlock (&self->lock);
        }
      break;

    default:
      break;
    }

  return TRUE;
}

/* -------------------------- opening files --------------------------------- */

/**
 * pt_player_open_uri:
 * @self: a #PtPlayer
 * @uri: the URI of the file
 *
 * Opens a local audio file for playback. It doesn’t work with videos or streams.
 * Only one file can be open at a time, playlists are not supported by the
 * backend. Opening a new file will close the previous one.
 *
 * When closing a file or on object destruction PtPlayer tries to write the
 * last position into the file’s metadata. On opening a file it reads the
 * metadata and jumps to the last known position if found.
 *
 * The player is set to the paused state and ready for playback. To start
 * playback use @pt_player_play().
 *
 * This operation blocks until it is finished. It returns TRUE on success or
 * FALSE on error. Errors are emitted async via #PtPlayer::error signal.
 *
 * Returns: TRUE if successful, otherwise FALSE
 *
 * Since: 2.0
 */
gboolean
pt_player_open_uri (PtPlayer *self,
                    gchar    *uri)
{
  GstBus *bus;
  gint64  dur = 0;

  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  /* If we had an open file before, remember its position */
  metadata_save_position (self);

  /* Reset any open streams */
  pt_player_clear (self);
  self->dur = -1;

  g_object_set (G_OBJECT (self->play), "uri", uri, NULL);

  /* setup message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (self->play));
  self->bus_watch_id = gst_bus_add_watch (bus, bus_call, self);
  gst_object_unref (bus);

  pt_player_pause (self);

  /* Block until state changed, return on failure */
  if (gst_element_get_state (self->play,
                             NULL, NULL,
                             GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_FAILURE)
    return FALSE;

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_CURRENT_URI]);

  gst_element_query_duration (self->play, GST_FORMAT_TIME, &dur);
  self->dur = dur;
  self->segstart = 0;
  self->segend = GST_CLOCK_TIME_NONE;
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE",
                    "Initial duration: %" GST_TIME_FORMAT, GST_TIME_ARGS (dur));

  metadata_goto_position (self);
  return TRUE;
}

/* ------------------------- Basic controls --------------------------------- */

/**
 * pt_player_pause:
 * @self: a #PtPlayer
 *
 * Sets the player to the paused state, meaning it stops playback and doesn’t
 * change position. To resume playback use @pt_player_play().
 *
 * Since: 1.4
 */
void
pt_player_pause (PtPlayer *self)
{
  GstState previous = self->current_state;

  g_return_if_fail (PT_IS_PLAYER (self));

  if (previous != GST_STATE_PAUSED)
    {
      self->target_state = GST_STATE_PAUSED;
      gst_element_set_state (self->play, GST_STATE_PAUSED);
    }

  if (previous == GST_STATE_PLAYING)
    g_signal_emit_by_name (self, "play-toggled");
}

/**
 * pt_player_pause_and_rewind:
 * @self: a #PtPlayer
 *
 * Like @pt_player_pause(), additionally rewinds the value of
 * #PtPlayer:pause in milliseconds.
 *
 * Since: 1.6
 */
void
pt_player_pause_and_rewind (PtPlayer *self)
{
  g_return_if_fail (PT_IS_PLAYER (self));

  pt_player_pause (self);
  pt_player_jump_relative (self, self->pause * -1);
}

/**
 * pt_player_get_pause:
 * @self: a #PtPlayer
 *
 * Returns the value of #PtPlayer:pause.
 *
 * Returns: time to rewind on pause in milliseconds
 *
 * Since: 1.6
 */
gint
pt_player_get_pause (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  return self->pause;
}

static void
pt_player_play_real (PtPlayer *self)
{
  GstState previous;

  previous = self->current_state;
  gst_element_set_state (self->play, GST_STATE_PLAYING);
  if (previous == GST_STATE_PAUSED)
    g_signal_emit_by_name (self, "play-toggled");
}

/**
 * pt_player_play:
 * @self: a #PtPlayer
 *
 * Starts playback in playback mode at the defined speed until it reaches the
 * end of stream (or the end of the selection). If the current position is at
 * the end, playback will start from the beginning of the stream or selection.
 *
 * In ASR mode it starts decoding the stream silently at the fastest possible
 * speed and emitting textual results via the #PtPlayer::asr-hypothesis and
 * #PtPlayer::asr-final signals.
 *
 * Since: 1.4
 */
void
pt_player_play (PtPlayer *self)
{
  gint64   pos;
  gint64   start, end;
  gboolean selection;

  g_return_if_fail (PT_IS_PLAYER (self));

  self->target_state = GST_STATE_PLAYING;
  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &pos))
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "pt_player_play: gst_element_query_position failed");
      pt_player_play_real (self);
      return;
    }

  if (GST_CLOCK_TIME_IS_VALID (self->segend))
    {
      selection = TRUE;
      start = self->segstart;
      end = self->segend;
    }
  else
    {
      selection = FALSE;
      start = 0;
      end = self->dur;
    }

  if (pos < start || pos > end)
    {
      pt_player_seek (self, start);
      /* Change to playing state will be performed on state change in bus_call() */
      return;
    }

  if (pos == end)
    {
      if ((selection && self->repeat_selection) || (!selection && self->repeat_all))
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Seek to start position");
          pt_player_seek (self, start);
        }
      else
        {
          pt_player_jump_relative (self, self->pause * -1);
        }
      /* Change to playing state will be performed on state change in bus_call() */
      return;
    }
  pt_player_play_real (self);
}

/**
 * pt_player_play_pause:
 * @self: a #PtPlayer
 *
 * Toggles between playback and pause, rewinds on pause.
 *
 * Since: 1.6
 */
void
pt_player_play_pause (PtPlayer *self)
{
  g_return_if_fail (PT_IS_PLAYER (self));

  switch (self->current_state)
    {
    case GST_STATE_PAUSED:
      pt_player_play (self);
      break;
    case GST_STATE_PLAYING:
      pt_player_pause_and_rewind (self);
      break;
    default:
      break;
    }
}

/**
 * pt_player_set_selection:
 * @self: a #PtPlayer
 * @start: selection start time in milliseconds
 * @end: selection end time in milliseconds
 *
 * Set a selection. If the current position is outside the selection, it will
 * be set to the selection’s start position, otherwise the current position is
 * not changed. Playing will end at the stop position and it’s not possible to
 * jump out of the selection until it is cleared with #pt_player_clear_selection.
 *
 * Since: 1.5
 */
void
pt_player_set_selection (PtPlayer *self,
                         gint64    start,
                         gint64    end)
{
  gint64 pos;

  g_return_if_fail (PT_IS_PLAYER (self));
  g_return_if_fail (start < end);
  g_return_if_fail (start >= 0);
  g_return_if_fail (end >= 0);

  if (self->segstart == GST_MSECOND * start &&
      self->segend == GST_MSECOND * (guint64) end)
    return;

  self->segstart = GST_MSECOND * start;
  self->segend = GST_MSECOND * end;

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &pos))
    return;

  if (pos < self->segstart || (guint64) pos > self->segend)
    pos = self->segstart;

  pt_player_seek (self, pos);
}

/**
 * pt_player_clear_selection:
 * @self: a #PtPlayer
 *
 * Clear and reset any selection.
 *
 * Since: 1.5
 */
void
pt_player_clear_selection (PtPlayer *self)
{
  gint64 pos;

  g_return_if_fail (PT_IS_PLAYER (self));

  if (self->segstart == 0 &&
      self->segend == GST_CLOCK_TIME_NONE)
    return;

  self->segstart = 0;
  self->segend = GST_CLOCK_TIME_NONE;

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &pos))
    return;

  pt_player_seek (self, pos);
}

/**
 * pt_player_has_selection:
 * @self: a #PtPlayer
 *
 * Returns whether there is currently a selection set or not.
 *
 * Returns: TRUE if there is a selection
 *
 * Since: 4.0
 */
gboolean
pt_player_has_selection (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);

  return (GST_CLOCK_TIME_IS_VALID (self->segend));
}

/* -------------------- Positioning, speed, volume -------------------------- */

/**
 * pt_player_jump_relative:
 * @self: a #PtPlayer
 * @milliseconds: time in milliseconds to jump
 *
 * Skips @milliseconds in stream. A positive value means jumping ahead. If the
 * resulting position would be beyond the end of stream (or selection), it goes
 * to the end of stream (or selection). A negative value means jumping back.
 * If the resulting position would be negative (or before the selection), it
 * jumps to position 0:00 (or to the start of the selection).
 *
 * Since: 1.4
 */
void
pt_player_jump_relative (PtPlayer *self,
                         gint      milliseconds)
{
  gint64 pos, new;

  g_return_if_fail (PT_IS_PLAYER (self));

  if (milliseconds == 0)
    return;

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &pos))
    return;

  new = pos + GST_MSECOND * milliseconds;
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Jump relative: dur = %" GST_TIME_FORMAT, GST_TIME_ARGS (self->dur));
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Jump relative: new = %" GST_TIME_FORMAT, GST_TIME_ARGS (new));

  if (new > self->dur)
    new = self->dur;

  if (GST_CLOCK_TIME_IS_VALID (self->segend) && (guint64) new > self->segend)
    new = self->segend;

  if (new < self->segstart)
    new = self->segstart;

  pt_player_seek (self, new);
}

/**
 * pt_player_jump_back:
 * @self: a #PtPlayer
 *
 * Jumps back the value of #PtPlayer:back.
 *
 * Since: 1.6
 */
void
pt_player_jump_back (PtPlayer *self)
{
  gint back;

  g_return_if_fail (PT_IS_PLAYER (self));

  back = self->back;
  if (back > 0)
    back = back * -1;
  pt_player_jump_relative (self, back);
  g_signal_emit_by_name (self, "jumped-back");
}

/**
 * pt_player_jump_forward:
 * @self: a #PtPlayer
 *
 * Jumps forward the value of #PtPlayer:forward.
 *
 * Since: 1.6
 */
void
pt_player_jump_forward (PtPlayer *self)
{
  g_return_if_fail (PT_IS_PLAYER (self));

  pt_player_jump_relative (self, self->forward);
  g_signal_emit_by_name (self, "jumped-forward");
}

/**
 * pt_player_get_back:
 * @self: a #PtPlayer
 *
 * Returns the value of #PtPlayer:back.
 *
 * Returns: time to jump back in milliseconds
 *
 * Since: 1.6
 */
gint
pt_player_get_back (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  return self->back;
}

/**
 * pt_player_get_forward:
 * @self: a #PtPlayer
 *
 * Returns the value of #PtPlayer:forward.
 *
 * Returns: time to jump forward in milliseconds
 *
 * Since: 1.6
 */
gint
pt_player_get_forward (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  return self->forward;
}

/**
 * pt_player_jump_to_position:
 * @self: a #PtPlayer
 * @milliseconds: position in milliseconds
 *
 * Jumps to a given position in stream. The position is given in @milliseconds
 * starting from position 0:00. A position beyond the duration of stream (or
 * outside the selection) is ignored.
 *
 * Since: 1.4
 */
void
pt_player_jump_to_position (PtPlayer *self,
                            gint      milliseconds)
{
  gint64 pos;

  g_return_if_fail (PT_IS_PLAYER (self));

  pos = GST_MSECOND * milliseconds;

  if (pos < 0)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Jump to position failed: negative value");
      return;
    }

  if (pos < self->segstart)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Setting position failed: target %" GST_TIME_FORMAT " before segstart %" GST_TIME_FORMAT,
                        GST_TIME_ARGS (pos), GST_TIME_ARGS (self->segstart));
      return;
    }
  if (GST_CLOCK_TIME_IS_VALID (self->segend) && (guint64) pos > self->segend)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Setting position failed: target %" GST_TIME_FORMAT " after segend %" GST_TIME_FORMAT,
                        GST_TIME_ARGS (pos), GST_TIME_ARGS (self->segend));
      return;
    }

  pt_player_seek (self, pos);
}

/**
 * pt_player_get_speed:
 * @self: a #PtPlayer
 *
 * Returns current playback speed.
 *
 * Returns: playback speed
 *
 * Since: 3.1
 */
gdouble
pt_player_get_speed (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 1);

  return self->speed;
}

static void
pt_player_set_speed_internal (PtPlayer *self)
{
  gint64 position;

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &position))
    return;
  self->seek_position = position;

  /* If there is no seek being dispatch to the main context currently do that,
   * otherwise we just updated the speed (rate) so that it will be taken by
   * the seek handler from the main context instead of the old one.
   */
  if (!self->seek_source)
    {
      /* If no seek is pending then create new seek source */
      if (!self->seek_pending)
        {
          self->seek_source = g_idle_source_new ();
          g_source_set_callback (self->seek_source,
                                 (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_source_attach (self->seek_source, NULL);
        }
    }
}

/**
 * pt_player_set_speed:
 * @self: a #PtPlayer
 * @speed: speed
 *
 * Sets the speed of playback in the paused state as well as during playback.
 * Normal speed is 1.0, everything above that is faster, everything below slower.
 * A speed of 0 is not allowed, use pt_player_pause() instead.
 * Recommended speed is starting from 0.5 as quality is rather poor below that.
 * Parlatype doesn’t change the pitch during slower or faster playback.
 *
 * Since: 1.4
 */
void
pt_player_set_speed (PtPlayer *self,
                     gdouble   speed)
{
  g_return_if_fail (PT_IS_PLAYER (self));
  g_return_if_fail (speed > 0);

  g_mutex_lock (&self->lock);
  if (speed == self->speed)
    {
      g_mutex_unlock (&self->lock);
      return;
    }
  self->speed = speed;
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Set speed=%f", speed);
  pt_player_set_speed_internal (self);
  g_mutex_unlock (&self->lock);

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_SPEED]);
}

/**
 * pt_player_get_volume:
 * @self: a #PtPlayer
 *
 * Gets the volume on a scale between 0 and 1.
 *
 * Returns: the current volume
 *
 * Since: 2.1
 */
gdouble
pt_player_get_volume (PtPlayer *self)
{
  gdouble volume;

  g_return_val_if_fail (PT_IS_PLAYER (self), -1);

  /* Pulseaudio sink does not propagate volume changes in GST_STATE_PAUSED
   * or lower. Ask for real value and update cached volume. */

  if (self->play)
    {
      g_object_get (self->play, "volume", &volume, NULL);
      self->volume = volume;
    }
  return gst_stream_volume_convert_volume (GST_STREAM_VOLUME_FORMAT_LINEAR,
                                           GST_STREAM_VOLUME_FORMAT_CUBIC,
                                           self->volume);
}

/**
 * pt_player_set_volume:
 * @self: a #PtPlayer
 * @volume: volume
 *
 * Sets the volume on a scale between 0 and 1. Instead of using this method
 * you could set the "volume" property.
 *
 * Since: 1.4
 */
void
pt_player_set_volume (PtPlayer *self,
                      gdouble   volume)
{
  g_return_if_fail (PT_IS_PLAYER (self));
  g_return_if_fail (volume >= 0 && volume <= 1);

  volume = gst_stream_volume_convert_volume (GST_STREAM_VOLUME_FORMAT_CUBIC,
                                             GST_STREAM_VOLUME_FORMAT_LINEAR,
                                             volume);
  if (volume == self->volume)
    return;

  self->volume = volume;

  if (self->play)
    g_object_set (self->play, "volume", volume, NULL);

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_VOLUME]);
}

/**
 * pt_player_get_mute:
 * @self: a #PtPlayer
 *
 * Get mute state of the audio stream.

 * Returns: TRUE for muted state, FALSE for normal state
 *
 * Since: 2.1
 */
gboolean
pt_player_get_mute (PtPlayer *self)
{
  gboolean retval = FALSE;

  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);

  if (self->play)
    g_object_get (self->play, "mute", &retval, NULL);

  return retval;
}

/**
 * pt_player_set_mute:
 * @self: a #PtPlayer
 * @mute: a gboolean
 *
 * Mute the player (with TRUE) or set it back to normal volume (with FALSE).
 * This remembers the volume level, so you don’t have to keep track of the old value.
 *
 * Since: 2.1
 */
void
pt_player_set_mute (PtPlayer *self,
                    gboolean  mute)
{
  g_return_if_fail (PT_IS_PLAYER (self));

  if (mute == self->mute)
    return;

  self->mute = mute;
  if (self->play)
    g_object_set (self->play, "mute", mute, NULL);
}

/**
 * pt_player_get_position:
 * @self: a #PtPlayer
 *
 * Returns the current position in stream.
 *
 * Returns: position in milliseconds or -1 on failure
 *
 * Since: 1.5
 */
gint64
pt_player_get_position (PtPlayer *self)
{
  gint64 time;

  g_return_val_if_fail (PT_IS_PLAYER (self), -1);

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &time))
    return -1;

  return GST_TIME_AS_MSECONDS (time);
}

/**
 * pt_player_get_duration:
 * @self: a #PtPlayer
 *
 * Returns the duration of stream.
 *
 * Returns: duration in milliseconds
 *
 * Since: 1.5
 */
gint64
pt_player_get_duration (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), -1);

  return GST_TIME_AS_MSECONDS (self->dur);
}

/* --------------------- Other widgets -------------------------------------- */

static void
wv_selection_changed_cb (GtkWidget *widget,
                         PtPlayer  *self)
{
  gint64 start, end;

  g_object_get (self->wv,
                "selection-start", &start,
                "selection-end", &end,
                NULL);

  if (start == end)
    pt_player_clear_selection (self);
  else
    pt_player_set_selection (self, start, end);
}

static void
wv_update_cursor (PtPlayer *self)
{
  g_object_set (self->wv,
                "playback-cursor",
                pt_player_get_position (self),
                NULL);
  if (self->set_follow_cursor)
    {
      pt_waveviewer_set_follow_cursor (self->wv, TRUE);
      self->set_follow_cursor = FALSE;
    }
}

static void
wv_cursor_changed_cb (PtWaveviewer *wv,
                      gint64        pos,
                      PtPlayer     *self)
{
  /* user changed cursor position */
  self->set_follow_cursor = TRUE;
  pt_player_jump_to_position (self, pos);
}

static void
wv_play_toggled_cb (GtkWidget *widget,
                    PtPlayer  *self)
{
  pt_player_play_pause (self);
}

/**
 * pt_player_connect_waveviewer:
 * @self: a #PtPlayer
 * @wv: a #PtWaveviewer
 *
 * Connect a #PtWaveviewer. The #PtPlayer will monitor selections made in the
 * #PtWaveviewer and act accordingly.
 *
 * Since: 1.6
 */
void
pt_player_connect_waveviewer (PtPlayer     *self,
                              PtWaveviewer *wv)
{
  g_return_if_fail (PT_IS_PLAYER (self));
  g_return_if_fail (PT_IS_WAVEVIEWER (wv));

  self->wv = wv;
  self->set_follow_cursor = FALSE;
  g_signal_connect (self->wv,
                    "selection-changed",
                    G_CALLBACK (wv_selection_changed_cb),
                    self);

  g_signal_connect (self->wv,
                    "cursor-changed",
                    G_CALLBACK (wv_cursor_changed_cb),
                    self);

  g_signal_connect (self,
                    "seek-done",
                    G_CALLBACK (wv_update_cursor),
                    NULL);

  g_signal_connect (self->wv,
                    "play-toggled",
                    G_CALLBACK (wv_play_toggled_cb),
                    self);
}

/* --------------------- File utilities ------------------------------------- */

/**
 * pt_player_get_uri:
 * @self: a #PtPlayer
 *
 * Returns the URI of the currently open file or NULL if it can’t be determined.
 *
 * Returns: (transfer full): the uri
 *
 * Since: 1.4
 */
gchar *
pt_player_get_uri (PtPlayer *self)
{
  gchar *uri = NULL;

  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  g_object_get (G_OBJECT (self->play), "current-uri", &uri, NULL);
  return uri;
}

/**
 * pt_player_get_filename:
 * @self: a #PtPlayer
 *
 * Returns the display name of the currently open file or NULL if it can’t be determined.
 *
 * Returns: (transfer full): the file name
 *
 * Since: 1.4
 */
gchar *
pt_player_get_filename (PtPlayer *self)
{
  GError      *error = NULL;
  const gchar *filename = NULL;
  GFile       *file = NULL;
  GFileInfo   *info = NULL;
  gchar       *result;

  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  file = pt_player_get_file (self);

  if (file)
    info = g_file_query_info (
        file,
        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
        G_FILE_QUERY_INFO_NONE,
        NULL,
        &error);
  else
    return NULL;

  if (error)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "File attributes not retrieved: %s", error->message);
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
 * pt_player_get_current_time_string:
 * @self: a #PtPlayer
 * @precision: a #PtPrecisionType
 *
 * Returns the current position of the stream as a string for display to the user.
 *
 * If the current position can not be determined, NULL is returned.
 *
 * Returns: (transfer full): the time string
 *
 * Since: 1.4
 */
gchar *
pt_player_get_current_time_string (PtPlayer       *self,
                                   PtPrecisionType precision)
{
  gint64 time;

  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);
  g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &time))
    return NULL;

  return pt_timestamp_to_string (
      GST_TIME_AS_MSECONDS (time),
      GST_TIME_AS_MSECONDS (self->dur),
      precision);
}

/**
 * pt_player_get_duration_time_string:
 * @self: a #PtPlayer
 * @precision: a #PtPrecisionType
 *
 * Returns the duration of the stream as a string for display to the user.
 *
 * If the duration can not be determined, NULL is returned.
 *
 * Returns: (transfer full): the time string
 *
 * Since: 1.4
 */
gchar *
pt_player_get_duration_time_string (PtPlayer       *self,
                                    PtPrecisionType precision)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);
  g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

  return pt_timestamp_to_string (
      GST_TIME_AS_MSECONDS (self->dur),
      GST_TIME_AS_MSECONDS (self->dur),
      precision);
}

/**
 * pt_player_export_timestamp:
 * @self: a #PtPlayer
 * @time: the time in milliseconds
 * @duration: duration in milliseconds
 *
 * Returns the timestamp for the given time as a string. Duration is needed
 * for some short time formats, the resulting timestamp format depends on
 * whether duration is less than one hour or more than (including) an hour
 * (3600000 milliseconds).
 *
 * The format of the timestamp can be influenced with
 * #PtPlayer:timestamp-precision, #PtPlayer:timestamp-fixed,
 * #PtPlayer:timestamp-fraction-sep and #PtPlayer:timestamp-delimiter.
 *
 * Returns: (transfer full): the timestamp
 *
 * Since: 1.6
 */
gchar *
pt_player_export_timestamp (PtPlayer *self,
                            gint      time,
                            gint      duration)
{
  gint   h, m, s, ms, mod, fraction;
  gchar *timestamp = NULL;

  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  /* This is the same code as in pt_player_get_timestring() */
  h = time / 3600000;
  mod = time % 3600000;
  m = mod / 60000;
  ms = time % 60000;
  s = ms / 1000;
  ms = ms % 1000;
  switch (self->timestamp_precision)
    {
    case PT_PRECISION_SECOND:
      fraction = -1;
      break;
    case PT_PRECISION_SECOND_10TH:
      fraction = ms / 100;
      break;
    case PT_PRECISION_SECOND_100TH:
      fraction = ms / 10;
      break;
    default:
      g_return_val_if_reached (NULL);
      break;
    }

  if (self->timestamp_fixed)
    {
      if (fraction >= 0)
        {
          if (self->timestamp_precision == PT_PRECISION_SECOND_10TH)
            {
              timestamp = g_strdup_printf ("%s%02d:%02d:%02d%s%d%s", self->timestamp_left, h, m, s, self->timestamp_sep, fraction, self->timestamp_right);
            }
          else
            {
              timestamp = g_strdup_printf ("%s%02d:%02d:%02d%s%02d%s", self->timestamp_left, h, m, s, self->timestamp_sep, fraction, self->timestamp_right);
            }
        }
      else
        {
          timestamp = g_strdup_printf ("%s%02d:%02d:%02d%s", self->timestamp_left, h, m, s, self->timestamp_right);
        }
    }
  else
    {
      if (fraction >= 0)
        {
          if (duration >= ONE_HOUR)
            {
              if (self->timestamp_precision == PT_PRECISION_SECOND_10TH)
                {
                  timestamp = g_strdup_printf ("%s%d:%02d:%02d%s%d%s", self->timestamp_left, h, m, s, self->timestamp_sep, fraction, self->timestamp_right);
                }
              else
                {
                  timestamp = g_strdup_printf ("%s%d:%02d:%02d%s%02d%s", self->timestamp_left, h, m, s, self->timestamp_sep, fraction, self->timestamp_right);
                }
            }
          else
            {
              if (self->timestamp_precision == PT_PRECISION_SECOND_10TH)
                {
                  timestamp = g_strdup_printf ("%s%d:%02d%s%d%s", self->timestamp_left, m, s, self->timestamp_sep, fraction, self->timestamp_right);
                }
              else
                {
                  timestamp = g_strdup_printf ("%s%d:%02d%s%02d%s", self->timestamp_left, m, s, self->timestamp_sep, fraction, self->timestamp_right);
                }
            }
        }
      else
        {
          if (duration >= ONE_HOUR)
            {
              timestamp = g_strdup_printf ("%s%d:%02d:%02d%s", self->timestamp_left, h, m, s, self->timestamp_right);
            }
          else
            {
              timestamp = g_strdup_printf ("%s%d:%02d%s", self->timestamp_left, m, s, self->timestamp_right);
            }
        }
    }

  return timestamp;
}

/**
 * pt_player_get_timestamp:
 * @self: a #PtPlayer
 *
 * Returns the current timestamp as a string. The format of the timestamp can
 * be influenced with #PtPlayer:timestamp-precision, #PtPlayer:timestamp-fixed,
 * #PtPlayer:timestamp-fraction-sep and #PtPlayer:timestamp-delimiter.
 *
 * If the current position can not be determined, NULL is returned.
 *
 * Returns: the timestamp
 *
 * Since: 1.4
 */
int
pt_player_get_timestamp (PtPlayer *self)
{
  gint64 time;

  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  if (!gst_element_query_position (self->play, GST_FORMAT_TIME, &time))
    return -1;

  return GST_TIME_AS_MSECONDS (time);
}

/**
 * pt_player_string_is_timestamp:
 * @self: a #PtPlayer
 * @timestamp: the string to be checked
 * @check_duration: whether timestamp’s time is less or equal stream’s duration
 *
 * Returns whether the given string is a valid timestamp. With @check_duration
 * FALSE it checks only for the formal validity of the timestamp. With
 * @check_duration TRUE the timestamp must be within the duration to be valid.
 *
 * See also pt_player_goto_timestamp() if you want to go to the timestamp’s
 * position immediately after.
 *
 * Returns: TRUE if the timestamp is valid, FALSE if not
 *
 * Since: 1.4
 */
gboolean
pt_player_string_is_timestamp (PtPlayer *self,
                               gchar    *string,
                               gboolean  check_duration)
{
  int timestamp;

  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  timestamp = pt_timestamp_from_string (string);
  if (timestamp > 0 && check_duration)
    {
      if (GST_MSECOND * (gint64) timestamp > self->dur)
        {
          timestamp = -1;
        }
    }

  return timestamp != -1;
}

/**
 * pt_player_goto_timestamp:
 * @self: a #PtPlayer
 * @timestamp: the timestamp to go to
 *
 * Goes to the position of the timestamp. Returns false, if it’s not a
 * valid timestamp.
 *
 * Returns: TRUE on success, FALSE if the timestamp is not valid
 *
 * Since: 1.4
 */
gboolean
pt_player_goto_timestamp (PtPlayer *self,
                          int       timestamp)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (timestamp >= 0, FALSE);

  if (GST_MSECOND * (gint64) timestamp > self->dur)
    {
      return FALSE;
    }

  pt_player_jump_to_position (self, timestamp);
  return TRUE;
}

/* --------------------- Init and GObject management ------------------------ */

static void
notify_volume_idle_cb (gpointer user_data)
{
  PtPlayer *self = PT_PLAYER (user_data);
  gdouble   vol;

  g_object_get (self->play, "volume", &vol, NULL);

  if (vol != self->volume)
    {
      self->volume = vol;
      g_object_notify_by_pspec (G_OBJECT (self),
                                obj_properties[PROP_VOLUME]);
    }

  self->vol_changed_id = 0;
}

static void
vol_changed (GObject    *object,
             GParamSpec *pspec,
             PtPlayer   *self)
{
  /* This is taken from Totem’s bacon-video-widget.c
     Changing the property immediately will crash, it has to be an idle source */

  self->vol_changed_id = g_idle_add_once (notify_volume_idle_cb, self);
  g_source_set_name_by_id (self->vol_changed_id, "[parlatype] notify_volume_idle_cb");
}

static void
notify_mute_idle_cb (gpointer user_data)
{
  PtPlayer *self = PT_PLAYER (user_data);
  gboolean  mute;

  g_object_get (self->play, "mute", &mute, NULL);

  if (mute != self->mute)
    {
      self->mute = mute;
      g_object_notify_by_pspec (G_OBJECT (self),
                                obj_properties[PROP_MUTE]);
    }

  self->mute_changed_id = 0;
}

static void
mute_changed (GObject    *object,
              GParamSpec *pspec,
              PtPlayer   *self)
{
  self->mute_changed_id = g_idle_add_once (notify_mute_idle_cb, self);
  g_source_set_name_by_id (self->mute_changed_id, "[parlatype] notify_mute_idle_cb");
}

static void
notify_uri_idle_cb (gpointer user_data)
{
  PtPlayer *self = PT_PLAYER (user_data);
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_CURRENT_URI]);
  self->uri_changed_id = 0;
}

static void
uri_changed (GObject    *object,
             GParamSpec *pspec,
             PtPlayer   *self)
{
  self->uri_changed_id = g_idle_add_once (notify_uri_idle_cb, self);
  g_source_set_name_by_id (self->uri_changed_id, "[parlatype] notify_uri_idle_cb");
}

/**
 * pt_player_set_mode:
 * @self: a #PtPlayer
 * @type: the desired output mode
 *
 * Set output mode. Initially #PtPlayer is in #PT_MODE_PLAYBACK. Before switching
 * to #PT_MODE_ASR the programmer has to call pt_player_configure_asr() and check
 * the result. Setting the mode is an asynchronous operation and when done in
 * paused state, it will happen only during the change to playing state.
 *
 * <note>Currently this works only well when done in paused state, in playing
 * state there are severe synchronisation issues.</note>
 *
 * To get the results in ASR mode, connect to the #PtPlayer::asr-hypothesis and/or
 * #PtPlayer::asr-final signal. Start recognition with pt_player_play().
 *
 * Returns: True on success.
 *
 * Since: 5.0
 */
gboolean
pt_player_set_mode (PtPlayer  *self,
                    PtModeType mode)
{
  GstPtAudioBin *bin = GST_PT_AUDIO_BIN (self->audio_bin);
  gint64         pos = 0;

  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);

  gst_element_query_position (self->play, GST_FORMAT_TIME, &pos);

  if (!gst_pt_audio_bin_set_mode (bin, mode))
    {
      return FALSE;
    }
  pt_player_seek (self, pos);
  return TRUE;
}

/**
 * pt_player_get_mode:
 * @self: a #PtPlayer
 *
 * Get current output mode.
 *
 * Returns: the current mode
 *
 * Since: 3.0
 */
PtModeType
pt_player_get_mode (PtPlayer *self)
{
  GstPtAudioBin *bin = GST_PT_AUDIO_BIN (self->audio_bin);

  g_return_val_if_fail (PT_IS_PLAYER (self), PT_MODE_INVALID);

  return gst_pt_audio_bin_get_mode (bin);
}

/**
 * pt_mode_get_name:
 * @mode: a #PtModeType
 *
 * Get the name for debugging purposes.
 *
 * Returns: the corresponding mode name
 *
 * Since: 3.0
 */
const char *
pt_mode_get_name (PtModeType mode)
{
  switch (mode)
    {
    case PT_MODE_PLAYBACK:
      return "play";
    case PT_MODE_ASR:
      return "asr";
    default:
      return "invalid";
    }
}

/**
 * pt_player_get_appsink:
 * @self: a `PtPlayer`
 *
 * Get a reference on the internal `GstAppSink` element to grab raw audio data.
 *
 * Link to GStreamer documentation: [GstAppSink](https://gstreamer.freedesktop.org/documentation/applib/gstappsink.html?gi-language=c)
 *
 * Returns: (transfer none): a `GstAppSink`
 */
GstElement *
pt_player_get_appsink (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  return gst_pt_audio_bin_get_appsink (GST_PT_AUDIO_BIN (self->audio_bin));
}

/**
 * pt_player_get_media_info:
 * @self: a #PtPlayer
 *
 * Get the player’s #PtMediaInfo object to query the current
 * track’s metadata.
 *
 * Returns: (transfer none): the player’s #PtMediaInfo object
 *
 * Since: 4.3
 */
PtMediaInfo *
pt_player_get_media_info (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  return self->info;
}

/**
 * pt_player_get_state:
 * @self: a `PtPlayer`
 *
 * Get the player's current state.
 *
 * Returns: an enum indicating the current state.
 */
PtStateType
pt_player_get_state (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  return self->app_state;
}

/**
 * pt_state_get_name:
 * @state: a #PtStateType
 *
 * Get the name for debugging purposes.
 *
 * Returns: the corresponding state name
 *
 * Since: 5.0
 */
const char *
pt_state_get_name (PtStateType state)
{
  switch (state)
    {
    case PT_STATE_STOPPED:
      return "stopped";
    case PT_STATE_PAUSED:
      return "paused";
    case PT_STATE_PLAYING:
      return "playing";
    case PT_STATE_TRANSCRIBING:
      return "transcribing";
    default:
      return "invalid";
    }
}

static void
pt_player_dispose (GObject *object)
{
  PtPlayer *self = PT_PLAYER (object);

  remove_seek_source (self);
  g_clear_signal_handler (&self->stream_notify_id, self->collection);
  g_clear_handle_id (&self->vol_changed_id, g_source_remove);
  g_clear_handle_id (&self->mute_changed_id, g_source_remove);
  g_clear_handle_id (&self->uri_changed_id, g_source_remove);
  g_clear_pointer (&self->collection, gst_object_unref);
  g_clear_object (&self->info);

  if (self->play)
    {
      /* remember position */
      metadata_save_position (self);
      g_clear_object (&self->pos_mgr);

      gst_element_set_state (self->play, GST_STATE_NULL);

      gst_object_unref (GST_OBJECT (self->play));
      self->play = NULL;
    }

  G_OBJECT_CLASS (pt_player_parent_class)->dispose (object);
}

static void
pt_player_finalize (GObject *object)
{
  PtPlayer *self = PT_PLAYER (object);

  g_mutex_clear (&self->lock);
  g_free (self->stream_id);

  G_OBJECT_CLASS (pt_player_parent_class)->finalize (object);
}

static void
pt_player_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  PtPlayer    *self = PT_PLAYER (object);
  const gchar *tmpchar;

  switch (property_id)
    {
    case PROP_SPEED:
      pt_player_set_speed (self, g_value_get_double (value));
      break;
    case PROP_VOLUME:
      pt_player_set_volume (self, g_value_get_double (value));
      break;
    case PROP_MUTE:
      pt_player_set_mute (self, g_value_get_boolean (value));
      break;
    case PROP_TIMESTAMP_PRECISION:
      self->timestamp_precision = g_value_get_int (value);
      break;
    case PROP_TIMESTAMP_FIXED:
      self->timestamp_fixed = g_value_get_boolean (value);
      break;
    case PROP_TIMESTAMP_DELIMITER:
      tmpchar = g_value_get_string (value);
      if (g_strcmp0 (tmpchar, "None") == 0)
        {
          self->timestamp_left = self->timestamp_right = "";
          break;
        }
      if (g_strcmp0 (tmpchar, "(") == 0)
        {
          self->timestamp_left = "(";
          self->timestamp_right = ")";
          break;
        }
      if (g_strcmp0 (tmpchar, "[") == 0)
        {
          self->timestamp_left = "[";
          self->timestamp_right = "]";
          break;
        }
      self->timestamp_left = self->timestamp_right = "#";
      break;
    case PROP_TIMESTAMP_FRACTION_SEP:
      tmpchar = g_value_get_string (value);
      if (g_strcmp0 (tmpchar, "-") == 0)
        {
          self->timestamp_sep = "-";
          break;
        }
      self->timestamp_sep = ".";
      break;
    case PROP_REWIND_ON_PAUSE:
      self->pause = g_value_get_int (value);
      break;
    case PROP_BACK:
      self->back = g_value_get_int (value);
      break;
    case PROP_FORWARD:
      self->forward = g_value_get_int (value);
      break;
    case PROP_REPEAT_ALL:
      self->repeat_all = g_value_get_boolean (value);
      break;
    case PROP_REPEAT_SELECTION:
      self->repeat_selection = g_value_get_boolean (value);
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
  PtPlayer *self = PT_PLAYER (object);
  char     *uri;

  switch (property_id)
    {
    case PROP_STATE:
      g_value_set_int (value, self->app_state);
      break;
    case PROP_CURRENT_URI:
      g_object_get (G_OBJECT (self->play), "current-uri", &uri, NULL);
      g_value_set_string (value, uri);
      g_free (uri);
      break;
    case PROP_SPEED:
      g_value_set_double (value, self->speed);
      break;
    case PROP_VOLUME:
      g_value_set_double (value, pt_player_get_volume (self));
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, self->mute);
      break;
    case PROP_TIMESTAMP_PRECISION:
      g_value_set_int (value, self->timestamp_precision);
      break;
    case PROP_TIMESTAMP_FIXED:
      g_value_set_boolean (value, self->timestamp_fixed);
      break;
    case PROP_TIMESTAMP_DELIMITER:
      g_value_set_string (value, self->timestamp_left);
      break;
    case PROP_TIMESTAMP_FRACTION_SEP:
      g_value_set_string (value, self->timestamp_sep);
      break;
    case PROP_REWIND_ON_PAUSE:
      g_value_set_int (value, self->pause);
      break;
    case PROP_BACK:
      g_value_set_int (value, self->back);
      break;
    case PROP_FORWARD:
      g_value_set_int (value, self->forward);
      break;
    case PROP_REPEAT_ALL:
      g_value_set_boolean (value, self->repeat_all);
      break;
    case PROP_REPEAT_SELECTION:
      g_value_set_boolean (value, self->repeat_selection);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_player_constructed (GObject *object)
{
  PtPlayer *self = PT_PLAYER (object);

  /* Setup pipeline */
  self->play = _pt_make_element ("playbin3", "play", NULL);
  self->scaletempo = _pt_make_element ("scaletempo", "tempo", NULL);
  self->audio_bin = _pt_make_element ("ptaudiobin", "audiobin", NULL);

  g_object_set (G_OBJECT (self->play),
                "audio-filter", self->scaletempo,
                "audio-sink", self->audio_bin, NULL);

  /* This is responsible for syncing system volume with Parlatype volume.
     Syncing is done only in Play state */
  g_signal_connect (G_OBJECT (self->play),
                    "notify::volume", G_CALLBACK (vol_changed), self);
  g_signal_connect (G_OBJECT (self->play),
                    "notify::mute", G_CALLBACK (mute_changed), self);

  /* Forward current-uri changes */
  g_signal_connect (G_OBJECT (self->play),
                    "notify::current-uri", G_CALLBACK (uri_changed), self);

  G_OBJECT_CLASS (pt_player_parent_class)->constructed (object);
}

static void
pt_player_init (PtPlayer *self)
{
  GstElementFactory *factory;

  self->timestamp_precision = PT_PRECISION_SECOND_10TH;
  self->timestamp_fixed = FALSE;
  self->wv = NULL;
  self->pos_mgr = pt_position_manager_new ();
  g_mutex_init (&self->lock);

  self->seek_pending = FALSE;
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->last_seek_time = GST_CLOCK_TIME_NONE;

  gst_init (NULL, NULL);

  /* Check if elements are already statically registered, otherwise
   * gst_element_get_factory() (used e.g. by playbin/decodebin) will
   * return an invalid factory. */
  factory = gst_element_factory_find ("ptaudiobin");
  if (factory == NULL)
    gst_pt_audio_bin_register ();
  else
    gst_object_unref (factory);

  self->current_state = GST_STATE_NULL;
  self->target_state = GST_STATE_NULL;
  self->speed = 1.0;
  self->info = _pt_media_info_new ();
}

static void
pt_player_class_init (PtPlayerClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = pt_player_set_property;
  G_OBJECT_CLASS (klass)->get_property = pt_player_get_property;
  G_OBJECT_CLASS (klass)->constructed = pt_player_constructed;
  G_OBJECT_CLASS (klass)->dispose = pt_player_dispose;
  G_OBJECT_CLASS (klass)->finalize = pt_player_finalize;

  /**
   * PtPlayer::end-of-stream:
   * @self: the player emitting the signal
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
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

  /**
   * PtPlayer::error:
   * @self: the player emitting the signal
   * @error: a GError
   *
   * The #PtPlayer::error signal is emitted on errors opening the file or during
   * playback. It’s a severe error and the player is always reset.
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
   * PtPlayer::play-toggled:
   * @self: the player emitting the signal
   *
   * The #PtPlayer::play-toggled signal is emitted when the player changed
   * to pause or play.
   */
  g_signal_new ("play-toggled",
                PT_TYPE_PLAYER,
                G_SIGNAL_RUN_FIRST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

  /**
   * PtPlayer::seek-done:
   * @self: the player emitting the signal
   *
   * The #PtPlayer::seek-done signal is emitted when a seek has finished successfully.
   * If several seeks are queued up, only the last one emits a signal.
   */
  g_signal_new ("seek-done",
                PT_TYPE_PLAYER,
                G_SIGNAL_RUN_FIRST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

  /**
   * PtPlayer::jumped-back:
   * @self: the player emitting the signal
   *
   * The #PtPlayer::jumped-back signal is emitted when the player jumped
   * back.
   */
  g_signal_new ("jumped-back",
                PT_TYPE_PLAYER,
                G_SIGNAL_RUN_FIRST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

  /**
   * PtPlayer::jumped-forward:
   * @self: the player emitting the signal
   *
   * The #PtPlayer::jumped-forward signal is emitted when the player jumped
   * forward.
   */
  g_signal_new ("jumped-forward",
                PT_TYPE_PLAYER,
                G_SIGNAL_RUN_FIRST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

  /**
   * PtPlayer::asr-final:
   * @self: the player emitting the signal
   * @word: recognized word(s)
   *
   * The #PtPlayer::asr-final signal is emitted in automatic speech recognition
   * mode whenever a word or a sequence of words was recognized and won’t change
   * anymore. For intermediate results see #PtPlayer::asr-hypothesis.
   */
  g_signal_new ("asr-final",
                PT_TYPE_PLAYER,
                G_SIGNAL_RUN_FIRST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE,
                1, G_TYPE_STRING);

  /**
   * PtPlayer::asr-hypothesis:
   * @self: the player emitting the signal
   * @word: probably recognized word(s)
   *
   * The #PtPlayer::asr-hypothesis signal is emitted in automatic speech recognition
   * mode as an intermediate result (hypothesis) of recognized words.
   * The hypothesis can still change. The programmer is responsible for
   * replacing an emitted hypothesis by either the next following hypothesis
   * or a following #PtPlayer::asr-final signal.
   *
   * It’s not necessary to connect to this signal if you want the final
   * result only. However, it can take a few seconds until a final result
   * is emitted and without an intermediate hypothesis the end user might
   * have the impression that there is nothing going on.
   */
  g_signal_new ("asr-hypothesis",
                PT_TYPE_PLAYER,
                G_SIGNAL_RUN_FIRST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE,
                1, G_TYPE_STRING);

  /**
   * PtPlayer:speed:
   *
   * The speed for playback.
   */
  obj_properties[PROP_SPEED] =
      g_param_spec_double (
          "speed", NULL, NULL,
          0.1, /* minimum */
          2.0, /* maximum */
          1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:volume:
   *
   * The volume for playback.
   *
   * <note><para>Pulseaudio sink does not propagate volume changes at GST_STATE_PAUSED
   * or lower. This property will notify of changes only in GST_STATE_PLAYING and
   * on state changes, e.g. from GST_STATE_READY to GST_STATE_PAUSED. Getting this
   * property returns the real current volume level even if there was no notification before.
   * </para></note>
   */
  obj_properties[PROP_VOLUME] =
      g_param_spec_double (
          "volume", NULL, NULL,
          0.0, /* minimum */
          1.0, /* maximum */
          1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:mute:
   *
   * Mute state of the audio stream.
   */
  obj_properties[PROP_MUTE] =
      g_param_spec_boolean (
          "mute", NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:timestamp-precision:
   *
   * How precise timestamps should be.
   */
  obj_properties[PROP_TIMESTAMP_PRECISION] =
      g_param_spec_int (
          "timestamp-precision", NULL, NULL,
          0, /* minimum = PT_PRECISION_SECOND */
          3, /* maximum = PT_PRECISION_INVALID */
          1,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:timestamp-fixed:
   *
   * Whether timestamp format should have a fixed number of digits.
   */
  obj_properties[PROP_TIMESTAMP_FIXED] =
      g_param_spec_boolean (
          "timestamp-fixed", NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:timestamp-delimiter:
   *
   * Character to delimit start and end of timestamp. Allowed values are
   * "None", hashtag "#", left bracket "(" and left square bracket "[".
   * PtPlayer will of course end with a right (square) bracket if those
   * are chosen. Any other character is changed to a hashtag "#".
   */
  obj_properties[PROP_TIMESTAMP_DELIMITER] =
      g_param_spec_string (
          "timestamp-delimiter", NULL, NULL,
          "#",
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:timestamp-fraction-sep:
   *
   * Character to separate fractions of a second from seconds. Only
   * point "." and minus "-" are allowed. Any other character is changed
   * to a point ".".
   */
  obj_properties[PROP_TIMESTAMP_FRACTION_SEP] =
      g_param_spec_string (
          "timestamp-fraction-sep", NULL, NULL,
          ".",
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:pause:
   *
   * Milliseconds to rewind on pause.
   */
  obj_properties[PROP_REWIND_ON_PAUSE] =
      g_param_spec_int (
          "pause", NULL, NULL,
          0,     /* minimum */
          10000, /* maximum */
          0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:back:
   *
   * Milliseconds to jump back.
   */
  obj_properties[PROP_BACK] =
      g_param_spec_int (
          "back", NULL, NULL,
          1000,  /* minimum */
          60000, /* maximum */
          10000,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:forward:
   *
   * Milliseconds to jump forward.
   */
  obj_properties[PROP_FORWARD] =
      g_param_spec_int (
          "forward", NULL, NULL,
          1000,  /* minimum */
          60000, /* maximum */
          10000,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:repeat-all:
   *
   * "Play" at the end of the file replays it.
   */
  obj_properties[PROP_REPEAT_ALL] =
      g_param_spec_boolean (
          "repeat-all", NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:repeat-selection:
   *
   * "Play" at the end of a selection replays it.
   */
  obj_properties[PROP_REPEAT_SELECTION] =
      g_param_spec_boolean (
          "repeat-selection", NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:state:
   *
   * The current state of PtPlayer.
   */
  obj_properties[PROP_STATE] =
      g_param_spec_int (
          "state", NULL, NULL,
          0, /* minimum = PT_STATE_STOPPED */
          2, /* maximum = PT_STATE_PLAYING */
          0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:current-uri:
   *
   * URI of the currently loaded stream.
   */
  obj_properties[PROP_CURRENT_URI] =
      g_param_spec_string (
          "current-uri", NULL, NULL,
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

/**
 * pt_player_new:
 *
 * Returns a new PtPlayer in #PT_MODE_PLAYBACK. The next step would be to open
 * a file with pt_player_open_uri().
 *
 * After use g_object_unref() it.
 *
 * Returns: (transfer full): a new pt_player
 *
 * Since: 1.6
 */
PtPlayer *
pt_player_new (void)
{
  _pt_i18n_init ();
  return g_object_new (PT_TYPE_PLAYER, NULL);
}
