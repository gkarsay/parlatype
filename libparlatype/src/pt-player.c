/* Copyright (C) Gabor Karsay 2016-2020 <gabor.karsay@gmx.at>
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
 * SECTION: pt-player
 * @short_description: The GStreamer backend for Parlatype.
 * @stability: Stable
 * @include: parlatype/pt-player.h
 *
 * PtPlayer is the GStreamer backend for Parlatype. Construct it with #pt_player_new().
 * Then you have to open a file with pt_player_open_uri().
 *
 * The internal time unit in PtPlayer are milliseconds.
 */

#define GETTEXT_PACKAGE GETTEXT_LIB

#include "config.h"

#include "gst/gst-helpers.h"
#include "gst/gstptaudiobin.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gst/audio/streamvolume.h>
#include <gst/gst.h>
#ifdef HAVE_POCKETSPHINX
#include "gst/gstparlasphinx.h"
#endif
#include "pt-config.h"
#include "pt-i18n.h"
#include "pt-player.h"
#include "pt-position-manager.h"
#include "pt-waveviewer.h"

typedef struct _PtPlayerPrivate PtPlayerPrivate;
struct _PtPlayerPrivate
{
  GstElement *play;
  GstElement *scaletempo;
  GstElement *audio_bin;
  guint       bus_watch_id;

  PtPositionManager *pos_mgr;
  GHashTable        *plugins;

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

  gint64 segstart;
  gint64 segend;

  GCancellable *c;

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
static void     remove_message_bus (PtPlayer *self);

G_DEFINE_TYPE_WITH_PRIVATE (PtPlayer, pt_player, G_TYPE_OBJECT)

/* -------------------------- static helpers -------------------------------- */

static void
pt_player_clear (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  remove_message_bus (self);
  priv->target_state = GST_STATE_NULL;
  priv->current_state = GST_STATE_NULL;
  gst_element_set_state (priv->play, GST_STATE_NULL);
}

static void
remove_seek_source (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  if (!priv->seek_source)
    return;

  g_source_destroy (priv->seek_source);
  g_source_unref (priv->seek_source);
  priv->seek_source = NULL;
}

static void
pt_player_seek_internal_locked (PtPlayer *self)
{
  PtPlayerPrivate     *priv = pt_player_get_instance_private (self);
  gboolean             ret;
  GstClockTime         position;
  gdouble              speed;
  gint64               stop;
  GstStateChangeReturn state_ret;

  remove_seek_source (self);

  if (priv->current_state < GST_STATE_PAUSED)
    return;

  if (priv->current_state != GST_STATE_PAUSED)
    {
      g_mutex_unlock (&priv->lock);
      state_ret = gst_element_set_state (priv->play, GST_STATE_PAUSED);
      if (state_ret == GST_STATE_CHANGE_FAILURE)
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
                            "MESSAGE", "Failed to seek");
          /* on error */
        }
      g_mutex_lock (&priv->lock);
      return;
    }

  priv->last_seek_time = gst_util_get_timestamp ();
  position = priv->seek_position;
  priv->seek_position = GST_CLOCK_TIME_NONE;
  priv->seek_pending = TRUE;
  speed = priv->speed;
  stop = priv->segend;
  g_mutex_unlock (&priv->lock);

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Seek to position %" GST_TIME_FORMAT ", stop at %" GST_TIME_FORMAT,
                    GST_TIME_ARGS (position), GST_TIME_ARGS (stop));

  ret = gst_element_seek (
      priv->play,
      speed,
      GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
      GST_SEEK_TYPE_SET,
      position,
      GST_SEEK_TYPE_SET,
      stop);

  if (!ret)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Failed to seek to %" GST_TIME_FORMAT,
                        GST_TIME_ARGS (position));
    }

  g_mutex_lock (&priv->lock);
}

static gboolean
pt_player_seek_internal (gpointer user_data)
{
  PtPlayer        *self = PT_PLAYER (user_data);
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  g_mutex_lock (&priv->lock);
  pt_player_seek_internal_locked (self);
  g_mutex_unlock (&priv->lock);

  return G_SOURCE_REMOVE;
}

static void
pt_player_seek (PtPlayer *self,
                gint64    position)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  g_mutex_lock (&priv->lock);

  priv->seek_position = position;

  if (!priv->seek_source)
    {
      GstClockTime now = gst_util_get_timestamp ();
      if (!priv->seek_pending || (now - priv->last_seek_time > 250 * GST_MSECOND))
        {
          priv->seek_source = g_idle_source_new ();
          g_source_set_callback (priv->seek_source, (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Dispatching seek to position %" GST_TIME_FORMAT,
                            GST_TIME_ARGS (position));
          g_source_attach (priv->seek_source, NULL);
        }
      else
        {
          guint delay = 250000 - (now - priv->last_seek_time) / 1000;
          priv->seek_source = g_timeout_source_new (delay);
          g_source_set_callback (priv->seek_source, (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_source_set_callback (priv->seek_source, (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Delaying seek to position %" GST_TIME_FORMAT "by %u microseconds",
                            GST_TIME_ARGS (position), delay);
          g_source_attach (priv->seek_source, NULL);
        }
    }

  g_mutex_unlock (&priv->lock);
}

static GFile *
pt_player_get_file (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gchar           *uri = NULL;
  GFile           *result = NULL;

  g_object_get (G_OBJECT (priv->play), "current-uri", &uri, NULL);

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
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  GFile           *file = NULL;
  gint64           pos;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos))
    return;

  file = pt_player_get_file (self);
  if (!file)
    return;

  pos = pos / GST_MSECOND;

  pt_position_manager_save (priv->pos_mgr, file, pos);
  g_object_unref (file);
}

static void
metadata_goto_position (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  GFile           *file = NULL;
  gint64           pos;

  file = pt_player_get_file (self);
  if (!file)
    return;

  pos = pt_position_manager_load (priv->pos_mgr, file);

  pt_player_jump_to_position (self, pos);
  g_object_unref (file);
}

const static gchar *
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
    case PT_STATE_INVALID:
      return NULL;
    }

  return NULL;
}

static void
change_app_state (PtPlayer   *self,
                  PtStateType state)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  if (state == priv->app_state)
    return;

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Changing app state from %s to %s",
                    pt_player_get_state_name (priv->app_state),
                    pt_player_get_state_name (state));

  priv->app_state = state;

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_STATE]);
}

static void
remove_message_bus (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  if (priv->bus_watch_id > 0)
    {
      g_source_remove (priv->bus_watch_id);
      priv->bus_watch_id = 0;
    }
}

static void
add_message_bus (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  GstBus          *bus;

  remove_message_bus (self);
  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->play));
  priv->bus_watch_id = gst_bus_add_watch (bus, bus_call, self);
  gst_object_unref (bus);
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  PtPlayer        *self = (PtPlayer *) data;
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           pos, stop;

  switch (GST_MESSAGE_TYPE (msg))
    {
    case GST_MESSAGE_EOS:
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE", "EOS");
      /* Sometimes the current position is not exactly at the end which looks like
       * a premature EOS or makes the cursor disappear.*/
      gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos);
      stop = GST_CLOCK_TIME_IS_VALID (priv->segend) ? priv->segend : priv->dur;
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
        gst_element_query_duration (priv->play, GST_FORMAT_TIME, &dur);
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE",
                          "New duration: %" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
        if (priv->dur != priv->segend)
          {
            priv->dur = dur;
          }
        else
          {
            priv->dur = priv->segend = dur;
            gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos);
            pt_player_seek (self, pos);
          }
        break;
      }
    case GST_MESSAGE_STATE_CHANGED:
      {
        if (GST_MESSAGE_SRC (msg) != GST_OBJECT (priv->play))
          break;
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

        priv->current_state = new_state;

        if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED && pending_state == GST_STATE_VOID_PENDING)
          {
            g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                              "MESSAGE", "Initial PAUSED - pre-rolled");
          }

        if (new_state == GST_STATE_PAUSED && pending_state == GST_STATE_VOID_PENDING)
          {
            g_mutex_lock (&priv->lock);
            if (priv->seek_pending)
              {
                priv->seek_pending = FALSE;
                if (priv->seek_source)
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
            if (priv->seek_position != GST_CLOCK_TIME_NONE)
              {
                g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "MESSAGE",
                                  "Seeking now that we reached PAUSED state");
                pt_player_seek_internal_locked (self);
                g_mutex_unlock (&priv->lock);
              }
            else if (!priv->seek_pending)
              {
                g_mutex_unlock (&priv->lock);
                if (priv->target_state >= GST_STATE_PLAYING)
                  {
                    GstStateChangeReturn state_ret;
                    state_ret = gst_element_set_state (priv->play, GST_STATE_PLAYING);
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
                g_mutex_unlock (&priv->lock);
              }
          }

        if (new_state == GST_STATE_PLAYING && pending_state == GST_STATE_VOID_PENDING)
          {
            if (!priv->seek_pending)
              change_app_state (self, PT_STATE_PLAYING);
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

        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          "MESSAGE", "Error from element %s: %s", GST_OBJECT_NAME (msg->src), error->message);
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          "MESSAGE", "Debugging info: %s", (debug) ? debug : "none");
        g_free (debug);

        g_signal_emit_by_name (self, "error", error);
        g_error_free (error);
        pt_player_clear (self);
        break;
      }

    case GST_MESSAGE_ELEMENT:
      if (g_strcmp0 (GST_MESSAGE_SRC_NAME (msg), "parlasphinx") == 0)
        {
          const GstStructure *st = gst_message_get_structure (msg);
          if (g_value_get_boolean (gst_structure_get_value (st, "final")))
            {
              g_signal_emit_by_name (self, "asr-final",
                                     g_value_get_string (
                                         gst_structure_get_value (st, "hypothesis")));
            }
          else
            {
              g_signal_emit_by_name (self, "asr-hypothesis",
                                     g_value_get_string (
                                         gst_structure_get_value (st, "hypothesis")));
            }
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
 * Return value: TRUE if successful, otherwise FALSE
 *
 * Since: 2.0
 */
gboolean
pt_player_open_uri (PtPlayer *self,
                    gchar    *uri)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  /* If we had an open file before, remember its position */
  metadata_save_position (self);

  /* Reset any open streams */
  pt_player_clear (self);
  priv->dur = -1;

  g_object_set (G_OBJECT (priv->play), "uri", uri, NULL);

  /* setup message handler */
  add_message_bus (self);

  pt_player_pause (self);

  /* Block until state changed, return on failure */
  if (gst_element_get_state (priv->play,
                             NULL, NULL,
                             GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_FAILURE)
    return FALSE;

  gint64 dur = 0;
  gst_element_query_duration (priv->play, GST_FORMAT_TIME, &dur);
  priv->dur = dur;
  priv->segstart = 0;
  priv->segend = GST_CLOCK_TIME_NONE;
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
  g_return_if_fail (PT_IS_PLAYER (self));

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  GstState         previous = priv->current_state;

  if (previous != GST_STATE_PAUSED)
    {
      priv->target_state = GST_STATE_PAUSED;
      gst_element_set_state (priv->play, GST_STATE_PAUSED);
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
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  pt_player_pause (self);
  pt_player_jump_relative (self, priv->pause * -1);
}

/**
 * pt_player_get_pause:
 * @self: a #PtPlayer
 *
 * Returns the value of #PtPlayer:pause.
 *
 * Return value: time to rewind on pause in milliseconds
 *
 * Since: 1.6
 */
gint
pt_player_get_pause (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  return priv->pause;
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
  g_return_if_fail (PT_IS_PLAYER (self));

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           pos;
  gint64           start, end;
  gboolean         selection;
  GstState         previous;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos))
    return;

  priv->target_state = GST_STATE_PLAYING;

  if (GST_CLOCK_TIME_IS_VALID (priv->segend))
    {
      selection = TRUE;
      start = priv->segstart;
      end = priv->segend;
    }
  else
    {
      selection = FALSE;
      start = 0;
      end = priv->dur;
    }

  if (pos < start || pos > end)
    {
      pt_player_seek (self, start);
      /* Change to playing state will be performed on state change in bus_call() */
      return;
    }

  if (pos == end)
    {
      if ((selection && priv->repeat_selection) || (!selection && priv->repeat_all))
        {
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                            "MESSAGE", "Seek to start position");
          pt_player_seek (self, start);
        }
      else
        {
          pt_player_jump_relative (self, priv->pause * -1);
        }
      /* Change to playing state will be performed on state change in bus_call() */
      return;
    }

  previous = priv->current_state;
  gst_element_set_state (priv->play, GST_STATE_PLAYING);
  if (previous == GST_STATE_PAUSED)
    g_signal_emit_by_name (self, "play-toggled");
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

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  switch (priv->current_state)
    {
    case GST_STATE_NULL:
      return;
    case GST_STATE_PAUSED:
      pt_player_play (self);
      break;
    case GST_STATE_PLAYING:
      pt_player_pause_and_rewind (self);
      break;
    case GST_STATE_VOID_PENDING:
      return;
    case GST_STATE_READY:
      return;
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
  g_return_if_fail (PT_IS_PLAYER (self));
  g_return_if_fail (start < end);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  priv->segstart = GST_MSECOND * start;
  priv->segend = GST_MSECOND * end;

  gint64 pos;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos))
    return;

  if (pos < priv->segstart || pos > priv->segend)
    pos = priv->segstart;

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
  g_return_if_fail (PT_IS_PLAYER (self));

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           pos;

  priv->segstart = 0;
  priv->segend = GST_CLOCK_TIME_NONE;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos))
    return;

  pt_player_seek (self, pos);
}

/**
 * pt_player_has_selection:
 * @self: a #PtPlayer
 *
 * Returns whether there is currently a selection set or not.
 *
 * Return value: TRUE if there is a selection
 *
 * Since: 4.0
 */
gboolean
pt_player_has_selection (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  return (GST_CLOCK_TIME_IS_VALID (priv->segend));
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
  g_return_if_fail (PT_IS_PLAYER (self));
  if (milliseconds == 0)
    return;

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           pos, new;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos))
    return;

  new = pos + GST_MSECOND *(gint64) milliseconds;
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Jump relative: dur = %" GST_TIME_FORMAT, GST_TIME_ARGS (priv->dur));
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Jump relative: new = %" GST_TIME_FORMAT, GST_TIME_ARGS (new));

  if (new > priv->dur)
    new = priv->dur;

  if (GST_CLOCK_TIME_IS_VALID (priv->segend) && new > priv->segend)
    new = priv->segend;

  if (new < priv->segstart)
    new = priv->segstart;

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
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint             back;

  back = priv->back;
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
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  pt_player_jump_relative (self, priv->forward);
  g_signal_emit_by_name (self, "jumped-forward");
}

/**
 * pt_player_get_back:
 * @self: a #PtPlayer
 *
 * Returns the value of #PtPlayer:back.
 *
 * Return value: time to jump back in milliseconds
 *
 * Since: 1.6
 */
gint
pt_player_get_back (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  return priv->back;
}

/**
 * pt_player_get_forward:
 * @self: a #PtPlayer
 *
 * Returns the value of #PtPlayer:forward.
 *
 * Return value: time to jump forward in milliseconds
 *
 * Since: 1.6
 */
gint
pt_player_get_forward (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 0);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  return priv->forward;
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
  g_return_if_fail (PT_IS_PLAYER (self));

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           pos;

  pos = GST_MSECOND * (gint64) milliseconds;

  if (pos < 0)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Jump to position failed: negative value");
      return;
    }

  if (pos < priv->segstart)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Setting position failed: target %" GST_TIME_FORMAT " before segstart %" GST_TIME_FORMAT,
                        GST_TIME_ARGS (pos), GST_TIME_ARGS (priv->segstart));
      return;
    }
  if (GST_CLOCK_TIME_IS_VALID (priv->segend) && pos > priv->segend)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Setting position failed: target %" GST_TIME_FORMAT " after segend %" GST_TIME_FORMAT,
                        GST_TIME_ARGS (pos), GST_TIME_ARGS (priv->segend));
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
 * Return value: playback speed
 *
 * Since: 3.1
 */
gdouble
pt_player_get_speed (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), 1);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  return priv->speed;
}

static void
pt_player_set_speed_internal (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           position;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &position))
    return;
  priv->seek_position = position;

  /* If there is no seek being dispatch to the main context currently do that,
   * otherwise we just updated the speed (rate) so that it will be taken by
   * the seek handler from the main context instead of the old one.
   */
  if (!priv->seek_source)
    {
      /* If no seek is pending then create new seek source */
      if (!priv->seek_pending)
        {
          priv->seek_source = g_idle_source_new ();
          g_source_set_callback (priv->seek_source,
                                 (GSourceFunc) pt_player_seek_internal, self, NULL);
          g_source_attach (priv->seek_source, NULL);
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

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  g_mutex_lock (&priv->lock);
  if (speed == priv->speed)
    {
      g_mutex_unlock (&priv->lock);
      return;
    }
  priv->speed = speed;
  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Set speed=%f", speed);
  pt_player_set_speed_internal (self);
  g_mutex_unlock (&priv->lock);

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_SPEED]);
}

/**
 * pt_player_get_volume:
 * @self: a #PtPlayer
 *
 * Gets the volume on a scale between 0 and 1.
 *
 * Return value: the current volume
 *
 * Since: 2.1
 */
gdouble
pt_player_get_volume (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), -1);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gdouble          volume;

  /* Pulseaudio sink does not propagate volume changes in GST_STATE_PAUSED
   * or lower. Ask for real value and update cached volume. */

  if (priv->play)
    {
      g_object_get (priv->play, "volume", &volume, NULL);
      priv->volume = volume;
    }
  return gst_stream_volume_convert_volume (GST_STREAM_VOLUME_FORMAT_LINEAR,
                                           GST_STREAM_VOLUME_FORMAT_CUBIC,
                                           priv->volume);
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

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  volume = gst_stream_volume_convert_volume (GST_STREAM_VOLUME_FORMAT_CUBIC,
                                             GST_STREAM_VOLUME_FORMAT_LINEAR,
                                             volume);
  if (volume == priv->volume)
    return;

  priv->volume = volume;

  if (priv->play)
    g_object_set (priv->play, "volume", volume, NULL);

  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_VOLUME]);
}

/**
 * pt_player_get_mute:
 * @self: a #PtPlayer
 *
 * Get mute state of the audio stream.

 * Return value: TRUE for muted state, FALSE for normal state
 *
 * Since: 2.1
 */
gboolean
pt_player_get_mute (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gboolean         retval = FALSE;

  if (priv->play)
    g_object_get (priv->play, "mute", &retval, NULL);

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

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  if (mute == priv->mute)
    return;

  priv->mute = mute;
  if (priv->play)
    g_object_set (priv->play, "mute", mute, NULL);
}

/**
 * pt_player_get_position:
 * @self: a #PtPlayer
 *
 * Returns the current position in stream.
 *
 * Return value: position in milliseconds or -1 on failure
 *
 * Since: 1.5
 */
gint64
pt_player_get_position (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), -1);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           time;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &time))
    return -1;

  return GST_TIME_AS_MSECONDS (time);
}

/**
 * pt_player_get_duration:
 * @self: a #PtPlayer
 *
 * Returns the duration of stream.
 *
 * Return value: duration in milliseconds
 *
 * Since: 1.5
 */
gint64
pt_player_get_duration (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), -1);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  return GST_TIME_AS_MSECONDS (priv->dur);
}

/* --------------------- Other widgets -------------------------------------- */

static void
wv_selection_changed_cb (GtkWidget *widget,
                         PtPlayer  *self)
{
  /* Selection changed in Waveviewer widget:
     - if we are not playing a selection: just set start/stop without seeking
     - if we are playing a selection and we are still in the selection:
       update selection
     - if we are playing a selection and the new one is somewhere else:
       stop playing the selection */

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           start, end, pos;
  g_object_get (priv->wv,
                "selection-start", &start,
                "selection-end", &end,
                NULL);

  if (priv->current_state == GST_STATE_PAUSED || !GST_CLOCK_TIME_IS_VALID (priv->segend))
    {
      priv->segstart = GST_MSECOND * start;
      priv->segend = (end == 0) ? GST_CLOCK_TIME_NONE : GST_MSECOND * end;
      return;
    }

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &pos))
    return;
  if (GST_MSECOND * start <= pos && pos <= GST_MSECOND * end)
    {
      pt_player_set_selection (self, start, end);
    }
  else
    {
      pt_player_clear_selection (self);
    }
}

static void
wv_update_cursor (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  g_object_set (priv->wv,
                "playback-cursor",
                pt_player_get_position (self),
                NULL);
  if (priv->set_follow_cursor)
    {
      pt_waveviewer_set_follow_cursor (priv->wv, TRUE);
      priv->set_follow_cursor = FALSE;
    }
}

static void
wv_cursor_changed_cb (PtWaveviewer *wv,
                      gint64        pos,
                      PtPlayer     *self)
{
  /* user changed cursor position */
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  priv->set_follow_cursor = TRUE;
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
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  priv->wv = wv;
  priv->set_follow_cursor = FALSE;
  g_signal_connect (priv->wv,
                    "selection-changed",
                    G_CALLBACK (wv_selection_changed_cb),
                    self);

  g_signal_connect (priv->wv,
                    "cursor-changed",
                    G_CALLBACK (wv_cursor_changed_cb),
                    self);

  g_signal_connect (self,
                    "seek-done",
                    G_CALLBACK (wv_update_cursor),
                    NULL);

  g_signal_connect (priv->wv,
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
 * Return value: (transfer full): the uri
 *
 * Since: 1.4
 */
gchar *
pt_player_get_uri (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gchar           *uri = NULL;
  g_object_get (G_OBJECT (priv->play), "current-uri", &uri, NULL);
  return uri;
}

/**
 * pt_player_get_filename:
 * @self: a #PtPlayer
 *
 * Returns the display name of the currently open file or NULL if it can’t be determined.
 *
 * Return value: (transfer full): the file name
 *
 * Since: 1.4
 */
gchar *
pt_player_get_filename (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  GError      *error = NULL;
  const gchar *filename = NULL;
  GFile       *file = NULL;
  GFileInfo   *info = NULL;
  gchar       *result;

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
 *
 * Since: 1.4
 */
gchar *
pt_player_get_time_string (gint            time,
                           gint            duration,
                           PtPrecisionType precision)
{
  /* Don’t assert time <= duration because duration is not exact */

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
  if (duration >= ONE_HOUR)
    {
      switch (precision)
        {
        case PT_PRECISION_SECOND:
          /* Translators: This is a time format, like "2:05:30" for 2
             hours, 5 minutes, and 30 seconds. You may change ":" to
             the separator that your locale uses or use "%Id" instead
             of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("long time format", "%d:%02d:%02d"), h, m, s);
          break;
        case PT_PRECISION_SECOND_10TH:
          /* Translators: This is a time format, like "2:05:30.1" for 2
             hours, 5 minutes, 30 seconds, and 1 tenthsecond. You may
             change ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("long time format, 1 digit", "%d:%02d:%02d.%d"), h, m, s, ms / 100);
          break;
        case PT_PRECISION_SECOND_100TH:
          /* Translators: This is a time format, like "2:05:30.12" for 2
             hours, 5 minutes, 30 seconds, and 12 hundrethseconds. You may
             change ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("long time format, 2 digits", "%d:%02d:%02d.%02d"), h, m, s, ms / 10);
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }

      return result;
    }

  if (duration >= TEN_MINUTES)
    {
      switch (precision)
        {
        case PT_PRECISION_SECOND:
          /* Translators: This is a time format, like "05:30" for
             5 minutes, and 30 seconds. You may change ":" to
             the separator that your locale uses or use "%I02d" instead
             of "%02d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("short time format", "%02d:%02d"), m, s);
          break;
        case PT_PRECISION_SECOND_10TH:
          /* Translators: This is a time format, like "05:30.1" for
             5 minutes, 30 seconds, and 1 tenthsecond. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("short time format, 1 digit", "%02d:%02d.%d"), m, s, ms / 100);
          break;
        case PT_PRECISION_SECOND_100TH:
          /* Translators: This is a time format, like "05:30.12" for
             5 minutes, 30 seconds, and 12 hundrethseconds. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("short time format, 2 digits", "%02d:%02d.%02d"), m, s, ms / 10);
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }
    }
  else
    {
      switch (precision)
        {
        case PT_PRECISION_SECOND:
          /* Translators: This is a time format, like "5:30" for
             5 minutes, and 30 seconds. You may change ":" to
             the separator that your locale uses or use "%Id" instead
             of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("shortest time format", "%d:%02d"), m, s);
          break;
        case PT_PRECISION_SECOND_10TH:
          /* Translators: This is a time format, like "05:30.1" for
             5 minutes, 30 seconds, and 1 tenthsecond. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("shortest time format, 1 digit", "%d:%02d.%d"), m, s, ms / 100);
          break;
        case PT_PRECISION_SECOND_100TH:
          /* Translators: This is a time format, like "05:30.12" for
             5 minutes, 30 seconds, and 12 hundrethseconds. You may change
             ":" or "." to the separator that your locale uses or
             use "%Id" instead of "%d" if your locale uses localized digits. */
          result = g_strdup_printf (C_ ("shortest time format, 2 digits", "%d:%02d.%02d"), m, s, ms / 10);
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }
    }

  return result;
}

/**
 * pt_player_get_current_time_string:
 * @self: a #PtPlayer
 * @precision: a #PtPrecisionType
 *
 * Returns the current position of the stream as a string for display to the user.
 *
 * If the current position can not be determined, NULL is returned.
 *
 * Return value: (transfer full): the time string
 *
 * Since: 1.4
 */
gchar *
pt_player_get_current_time_string (PtPlayer       *self,
                                   PtPrecisionType precision)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);
  g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           time;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &time))
    return NULL;

  return pt_player_get_time_string (
      GST_TIME_AS_MSECONDS (time),
      GST_TIME_AS_MSECONDS (priv->dur),
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
 * Return value: (transfer full): the time string
 *
 * Since: 1.4
 */
gchar *
pt_player_get_duration_time_string (PtPlayer       *self,
                                    PtPrecisionType precision)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);
  g_return_val_if_fail (precision < PT_PRECISION_INVALID, NULL);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  return pt_player_get_time_string (
      GST_TIME_AS_MSECONDS (priv->dur),
      GST_TIME_AS_MSECONDS (priv->dur),
      precision);
}

/**
 * pt_player_get_timestamp_for_time:
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
 * Return value: (transfer full): the timestamp
 *
 * Since: 1.6
 */
gchar *
pt_player_get_timestamp_for_time (PtPlayer *self,
                                  gint      time,
                                  gint      duration)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint             h, m, s, ms, mod, fraction;
  gchar           *timestamp = NULL;

  /* This is the same code as in pt_player_get_timestring() */
  h = time / 3600000;
  mod = time % 3600000;
  m = mod / 60000;
  ms = time % 60000;
  s = ms / 1000;
  ms = ms % 1000;
  switch (priv->timestamp_precision)
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

  if (priv->timestamp_fixed)
    {
      if (fraction >= 0)
        {
          if (priv->timestamp_precision == PT_PRECISION_SECOND_10TH)
            {
              timestamp = g_strdup_printf ("%s%02d:%02d:%02d%s%d%s", priv->timestamp_left, h, m, s, priv->timestamp_sep, fraction, priv->timestamp_right);
            }
          else
            {
              timestamp = g_strdup_printf ("%s%02d:%02d:%02d%s%02d%s", priv->timestamp_left, h, m, s, priv->timestamp_sep, fraction, priv->timestamp_right);
            }
        }
      else
        {
          timestamp = g_strdup_printf ("%s%02d:%02d:%02d%s", priv->timestamp_left, h, m, s, priv->timestamp_right);
        }
    }
  else
    {
      if (fraction >= 0)
        {
          if (duration >= ONE_HOUR)
            {
              if (priv->timestamp_precision == PT_PRECISION_SECOND_10TH)
                {
                  timestamp = g_strdup_printf ("%s%d:%02d:%02d%s%d%s", priv->timestamp_left, h, m, s, priv->timestamp_sep, fraction, priv->timestamp_right);
                }
              else
                {
                  timestamp = g_strdup_printf ("%s%d:%02d:%02d%s%02d%s", priv->timestamp_left, h, m, s, priv->timestamp_sep, fraction, priv->timestamp_right);
                }
            }
          else
            {
              if (priv->timestamp_precision == PT_PRECISION_SECOND_10TH)
                {
                  timestamp = g_strdup_printf ("%s%d:%02d%s%d%s", priv->timestamp_left, m, s, priv->timestamp_sep, fraction, priv->timestamp_right);
                }
              else
                {
                  timestamp = g_strdup_printf ("%s%d:%02d%s%02d%s", priv->timestamp_left, m, s, priv->timestamp_sep, fraction, priv->timestamp_right);
                }
            }
        }
      else
        {
          if (duration >= ONE_HOUR)
            {
              timestamp = g_strdup_printf ("%s%d:%02d:%02d%s", priv->timestamp_left, h, m, s, priv->timestamp_right);
            }
          else
            {
              timestamp = g_strdup_printf ("%s%d:%02d%s", priv->timestamp_left, m, s, priv->timestamp_right);
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
 * Return value: (transfer full): the timestamp
 *
 * Since: 1.4
 */
gchar *
pt_player_get_timestamp (PtPlayer *self)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), NULL);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint64           time;
  gint             duration;

  if (!gst_element_query_position (priv->play, GST_FORMAT_TIME, &time))
    return NULL;

  duration = GST_TIME_AS_MSECONDS (priv->dur);

  return pt_player_get_timestamp_for_time (self, GST_TIME_AS_MSECONDS (time), duration);
}

/**
 * pt_player_get_timestamp_position:
 * @self: a #PtPlayer
 * @timestamp: the timestamp
 * @check_duration: checking the timestamp’s validity also check duration
 *
 * Returns the time in milliseconds represented by the timestamp or -1 for
 * invalid timestamps.
 *
 * Return value: the time in milliseconds represented by the timestamp or -1
 * for invalid timestamps
 *
 * Since: 1.6
 */
gint
pt_player_get_timestamp_position (PtPlayer *self,
                                  gchar    *timestamp,
                                  gboolean  check_duration)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gint             h, m, s, ms, result;
  gchar           *cmp; /* timestamp without delimiters */
  gboolean         long_format;
  gboolean         fraction;
  gchar          **split = NULL;
  guint            n_split;

  /* Check for formal validity */
  if (!g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9]:[0-9][0-9][\\.|\\-][0-9][0-9]?[#|\\)|\\]]?$", timestamp, 0, 0) && !g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9][\\.|\\-][0-9][0-9]?[#|\\)|\\]]?$", timestamp, 0, 0) && !g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9]:[0-9][0-9][#|\\)|\\]]?$", timestamp, 0, 0) && !g_regex_match_simple ("^[#|\\(|\\[]?[0-9][0-9]?:[0-9][0-9][#|\\)|\\]]?$", timestamp, 0, 0))
    {
      return -1;
    }

  /* Delimiters must match */
  if (g_str_has_prefix (timestamp, "#") && !g_str_has_suffix (timestamp, "#"))
    return -1;
  if (g_str_has_prefix (timestamp, "(") && !g_str_has_suffix (timestamp, ")"))
    return -1;
  if (g_str_has_prefix (timestamp, "[") && !g_str_has_suffix (timestamp, "]"))
    return -1;
  if (g_regex_match_simple ("^[0-9]", timestamp, 0, 0))
    {
      if (!g_regex_match_simple ("[0-9]$", timestamp, 0, 0))
        return -1;
    }

  /* Remove delimiters */
  if (g_str_has_prefix (timestamp, "#") || g_str_has_prefix (timestamp, "(") || g_str_has_prefix (timestamp, "["))
    {
      timestamp++;
      cmp = g_strdup_printf ("%.*s", (int) strlen (timestamp) - 1, timestamp);
    }
  else
    {
      cmp = g_strdup (timestamp);
    }

  /* Determine format and split timestamp into h, m, s, ms */
  h = 0;
  ms = 0;

  long_format = g_regex_match_simple (":[0-9][0-9]:", cmp, 0, 0);
  fraction = !g_regex_match_simple (".*:[0-9][0-9]$", cmp, 0, 0);
  split = g_regex_split_simple ("[:|\\.|\\-]", cmp, 0, 0);
  g_free (cmp);
  if (!split)
    return -1;

  n_split = 2;
  if (long_format)
    n_split += 1;
  if (fraction)
    n_split += 1;
  if (n_split != g_strv_length (split))
    {
      g_strfreev (split);
      return -1;
    }

  if (long_format)
    {
      h = (int) g_ascii_strtoull (split[0], NULL, 10);
      m = (int) g_ascii_strtoull (split[1], NULL, 10);
      s = (int) g_ascii_strtoull (split[2], NULL, 10);
      if (fraction)
        {
          ms = (int) g_ascii_strtoull (split[3], NULL, 10);
          if (strlen (split[3]) == 1)
            ms = ms * 100;
          else
            ms = ms * 10;
        }
    }
  else
    {
      m = (int) g_ascii_strtoull (split[0], NULL, 10);
      s = (int) g_ascii_strtoull (split[1], NULL, 10);
      if (fraction)
        {
          ms = (int) g_ascii_strtoull (split[2], NULL, 10);
          if (strlen (split[2]) == 1)
            ms = ms * 100;
          else
            ms = ms * 10;
        }
    }

  g_strfreev (split);

  /* Sanity check */
  if (s > 59 || m > 59)
    return -1;

  result = (h * 3600 + m * 60 + s) * 1000 + ms;

  if (check_duration)
    {
      if (GST_MSECOND * (gint64) result > priv->dur)
        {
          return -1;
        }
    }

  return result;
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
 * Return value: TRUE if the timestamp is valid, FALSE if not
 *
 * Since: 1.4
 */
gboolean
pt_player_string_is_timestamp (PtPlayer *self,
                               gchar    *timestamp,
                               gboolean  check_duration)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (timestamp != NULL, FALSE);

  return (pt_player_get_timestamp_position (self, timestamp, check_duration) != -1);
}

/**
 * pt_player_goto_timestamp:
 * @self: a #PtPlayer
 * @timestamp: the timestamp to go to
 *
 * Goes to the position of the timestamp. Returns false, if it’s not a
 * valid timestamp.
 *
 * Return value: TRUE on success, FALSE if the timestamp is not valid
 *
 * Since: 1.4
 */
gboolean
pt_player_goto_timestamp (PtPlayer *self,
                          gchar    *timestamp)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (timestamp != NULL, FALSE);

  gint pos;

  pos = pt_player_get_timestamp_position (self, timestamp, TRUE);

  if (pos == -1)
    return FALSE;

  pt_player_jump_to_position (self, pos);
  return TRUE;
}

/* --------------------- Init and GObject management ------------------------ */

static gboolean
notify_volume_idle_cb (PtPlayer *self)
{
  if (!PT_IS_PLAYER (self))
    return G_SOURCE_REMOVE;

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gdouble          vol;

  g_object_get (priv->play, "volume", &vol, NULL);

  if (vol != priv->volume)
    {
      priv->volume = vol;
      g_object_notify_by_pspec (G_OBJECT (self),
                                obj_properties[PROP_VOLUME]);
    }

  return FALSE;
}

static void
vol_changed (GObject    *object,
             GParamSpec *pspec,
             PtPlayer   *self)
{
  /* This is taken from Totem’s bacon-video-widget.c
     Changing the property immediately will crash, it has to be an idle source */

  guint id;
  id = g_idle_add ((GSourceFunc) notify_volume_idle_cb, self);
  g_source_set_name_by_id (id, "[parlatype] notify_volume_idle_cb");
}

static gboolean
notify_mute_idle_cb (PtPlayer *self)
{
  if (!PT_IS_PLAYER (self))
    return G_SOURCE_REMOVE;

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gboolean         mute;

  g_object_get (priv->play, "mute", &mute, NULL);

  if (mute != priv->mute)
    {
      priv->mute = mute;
      g_object_notify_by_pspec (G_OBJECT (self),
                                obj_properties[PROP_MUTE]);
    }

  return FALSE;
}

static void
mute_changed (GObject    *object,
              GParamSpec *pspec,
              PtPlayer   *self)
{
  guint id;
  id = g_idle_add ((GSourceFunc) notify_mute_idle_cb, self);
  g_source_set_name_by_id (id, "[parlatype] notify_mute_idle_cb");
}

static gboolean
notify_uri_idle_cb (PtPlayer *self)
{
  g_object_notify_by_pspec (G_OBJECT (self),
                            obj_properties[PROP_CURRENT_URI]);
  return FALSE;
}

static void
uri_changed (GObject    *object,
             GParamSpec *pspec,
             PtPlayer   *self)
{
  guint id;
  id = g_idle_add ((GSourceFunc) notify_uri_idle_cb, self);
  g_source_set_name_by_id (id, "[parlatype] notify_uri_idle_cb");
}

/**
 * pt_player_configure_asr:
 * @self: a #PtPlayer
 * @config: a #PtConfig
 * @error: (nullable): return location for an error, or NULL
 *
 * Configure ASR setup with a #PtConfig instance. This is necessary before
 * switching into #PT_MODE_ASR the first time. The configuration remains if
 * you switch to normal playback and back to ASR again. Subsequently it can
 * be changed in both modes.
 *
 * Return value: TRUE on success, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_player_configure_asr (PtPlayer *self,
                         PtConfig *config,
                         GError  **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gboolean         result;
  GstPtAudioBin   *bin;

  bin = GST_PT_AUDIO_BIN (priv->audio_bin);
  result = gst_pt_audio_bin_configure_asr (bin, config, error);

  return result;
}

/**
 * pt_player_config_is_loadable:
 * @self: a #PtPlayer
 * @config: a #PtConfig
 *
 * Checks if #PtPlayer is able to load the GStreamer plugin used by @config
 * without actually loading it. It does not guarantee that the plugin is
 * functional, merely that it is either a private plugin of PtPlayer itself
 * or an installed external plugin.
 *
 * Return value: TRUE on success, otherwise FALSE
 *
 * Since: 3.0
 */
gboolean
pt_player_config_is_loadable (PtPlayer *self,
                              PtConfig *config)
{
  g_return_val_if_fail (PT_IS_PLAYER (self), FALSE);
  g_return_val_if_fail (PT_IS_CONFIG (config), FALSE);

  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  gchar           *plugin_name;
  GstElement      *plugin;
  gpointer         pointer;
  gboolean         result;

  plugin_name = pt_config_get_plugin (config);
  if (!plugin_name)
    return FALSE;

  if (g_hash_table_contains (priv->plugins, plugin_name))
    {
      pointer = g_hash_table_lookup (priv->plugins,
                                     plugin_name);
      return GPOINTER_TO_INT (pointer);
    }

  plugin = gst_element_factory_make (plugin_name, NULL);

  if (plugin)
    {
      gst_object_unref (plugin);
      result = TRUE;
    }
  else
    {
      result = FALSE;
    }

  g_hash_table_insert (priv->plugins,
                       g_strdup (plugin_name), GINT_TO_POINTER (result));

  return result;
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
 * Since: 3.0
 */
void
pt_player_set_mode (PtPlayer  *self,
                    PtModeType type)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  GstPtAudioBin   *bin = GST_PT_AUDIO_BIN (priv->audio_bin);
  gst_pt_audio_bin_set_mode (bin, type);
}

/**
 * pt_player_get_mode:
 * @self: a #PtPlayer
 *
 * Get current output mode.
 *
 * Return value: the current mode
 *
 * Since: 3.0
 */
PtModeType
pt_player_get_mode (PtPlayer *self)
{
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  GstPtAudioBin   *bin = GST_PT_AUDIO_BIN (priv->audio_bin);
  return gst_pt_audio_bin_get_mode (bin);
}

static void
pt_player_constructed (GObject *object)
{
  PtPlayer        *self = PT_PLAYER (object);
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  /* This is responsible for syncing system volume with Parlatype volume.
     Syncing is done only in Play state */
  g_signal_connect (G_OBJECT (priv->play),
                    "notify::volume", G_CALLBACK (vol_changed), self);
  g_signal_connect (G_OBJECT (priv->play),
                    "notify::mute", G_CALLBACK (mute_changed), self);

  /* Forward current-uri changes */
  g_signal_connect (G_OBJECT (priv->play),
                    "notify::current-uri", G_CALLBACK (uri_changed), self);

  G_OBJECT_CLASS (pt_player_parent_class)->constructed (object);
}

static void
pt_player_dispose (GObject *object)
{
  PtPlayer        *self = PT_PLAYER (object);
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  remove_seek_source (self);
  if (priv->play)
    {
      /* remember position */
      metadata_save_position (self);
      g_clear_object (&priv->pos_mgr);

      gst_element_set_state (priv->play, GST_STATE_NULL);

      gst_object_unref (GST_OBJECT (priv->play));
      priv->play = NULL;

      g_hash_table_destroy (priv->plugins);
    }

  G_OBJECT_CLASS (pt_player_parent_class)->dispose (object);
}

static void
pt_player_finalize (GObject *object)
{
  PtPlayer        *self = PT_PLAYER (object);
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (pt_player_parent_class)->finalize (object);
}

static void
pt_player_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  PtPlayer        *self = PT_PLAYER (object);
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  const gchar     *tmpchar;

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
      priv->timestamp_precision = g_value_get_int (value);
      break;
    case PROP_TIMESTAMP_FIXED:
      priv->timestamp_fixed = g_value_get_boolean (value);
      break;
    case PROP_TIMESTAMP_DELIMITER:
      tmpchar = g_value_get_string (value);
      if (g_strcmp0 (tmpchar, "None") == 0)
        {
          priv->timestamp_left = priv->timestamp_right = "";
          break;
        }
      if (g_strcmp0 (tmpchar, "(") == 0)
        {
          priv->timestamp_left = "(";
          priv->timestamp_right = ")";
          break;
        }
      if (g_strcmp0 (tmpchar, "[") == 0)
        {
          priv->timestamp_left = "[";
          priv->timestamp_right = "]";
          break;
        }
      priv->timestamp_left = priv->timestamp_right = "#";
      break;
    case PROP_TIMESTAMP_FRACTION_SEP:
      tmpchar = g_value_get_string (value);
      if (g_strcmp0 (tmpchar, "-") == 0)
        {
          priv->timestamp_sep = "-";
          break;
        }
      priv->timestamp_sep = ".";
      break;
    case PROP_REWIND_ON_PAUSE:
      priv->pause = g_value_get_int (value);
      break;
    case PROP_BACK:
      priv->back = g_value_get_int (value);
      break;
    case PROP_FORWARD:
      priv->forward = g_value_get_int (value);
      break;
    case PROP_REPEAT_ALL:
      priv->repeat_all = g_value_get_boolean (value);
      break;
    case PROP_REPEAT_SELECTION:
      priv->repeat_selection = g_value_get_boolean (value);
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
  PtPlayer        *self = PT_PLAYER (object);
  PtPlayerPrivate *priv = pt_player_get_instance_private (self);
  char            *uri;

  switch (property_id)
    {
    case PROP_STATE:
      g_value_set_int (value, priv->app_state);
      break;
    case PROP_CURRENT_URI:
      g_object_get (G_OBJECT (priv->play), "current-uri", &uri, NULL);
      g_value_set_string (value, uri);
      g_free (uri);
      break;
    case PROP_SPEED:
      g_value_set_double (value, priv->speed);
      break;
    case PROP_VOLUME:
      g_value_set_double (value, pt_player_get_volume (self));
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, priv->mute);
      break;
    case PROP_TIMESTAMP_PRECISION:
      g_value_set_int (value, priv->timestamp_precision);
      break;
    case PROP_TIMESTAMP_FIXED:
      g_value_set_boolean (value, priv->timestamp_fixed);
      break;
    case PROP_TIMESTAMP_DELIMITER:
      g_value_set_string (value, priv->timestamp_left);
      break;
    case PROP_TIMESTAMP_FRACTION_SEP:
      g_value_set_string (value, priv->timestamp_sep);
      break;
    case PROP_REWIND_ON_PAUSE:
      g_value_set_int (value, priv->pause);
      break;
    case PROP_BACK:
      g_value_set_int (value, priv->back);
      break;
    case PROP_FORWARD:
      g_value_set_int (value, priv->forward);
      break;
    case PROP_REPEAT_ALL:
      g_value_set_boolean (value, priv->repeat_all);
      break;
    case PROP_REPEAT_SELECTION:
      g_value_set_boolean (value, priv->repeat_selection);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_player_init (PtPlayer *self)
{
  PtPlayerPrivate   *priv = pt_player_get_instance_private (self);
  GstElementFactory *factory;

  priv->timestamp_precision = PT_PRECISION_SECOND_10TH;
  priv->timestamp_fixed = FALSE;
  priv->wv = NULL;
  priv->pos_mgr = pt_position_manager_new ();
  priv->plugins = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, NULL);
  g_mutex_init (&priv->lock);

  priv->seek_pending = FALSE;
  priv->seek_position = GST_CLOCK_TIME_NONE;
  priv->last_seek_time = GST_CLOCK_TIME_NONE;

  gst_init (NULL, NULL);

  /* Check if elements are already statically registered, otherwise
   * gst_element_get_factory() (used e.g. by playbin/decodebin) will
   * return an invalid factory. */
  factory = gst_element_factory_find ("ptaudiobin");
  if (factory == NULL)
    gst_pt_audio_bin_register ();
  else
    gst_object_unref (factory);
#ifdef HAVE_POCKETSPHINX
  factory = gst_element_factory_find ("parlasphinx");
  if (factory == NULL)
    gst_parlasphinx_register ();
  else
    gst_object_unref (factory);
#endif

  priv->play = _pt_make_element ("playbin", "play", NULL);
  priv->scaletempo = _pt_make_element ("scaletempo", "tempo", NULL);
  priv->audio_bin = _pt_make_element ("ptaudiobin", "audiobin", NULL);

  g_object_set (G_OBJECT (priv->play),
                "audio-filter", priv->scaletempo,
                "audio-sink", priv->audio_bin, NULL);

  priv->current_state = GST_STATE_NULL;
  priv->target_state = GST_STATE_NULL;
}

static void
pt_player_class_init (PtPlayerClass *klass)
{
  G_OBJECT_CLASS (klass)->set_property = pt_player_set_property;
  G_OBJECT_CLASS (klass)->get_property = pt_player_get_property;
  G_OBJECT_CLASS (klass)->dispose = pt_player_constructed;
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
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

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
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * PtPlayer:mute:
   *
   * Mute state of the audio stream.
   */
  obj_properties[PROP_MUTE] =
      g_param_spec_boolean (
          "mute", NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

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
 * Return value: (transfer full): a new pt_player
 *
 * Since: 1.6
 */
PtPlayer *
pt_player_new (void)
{
  _pt_i18n_init ();
  return g_object_new (PT_TYPE_PLAYER, NULL);
}
