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
 */

#pragma once

#if !defined(__PARLATYPE_H_INSIDE__) && !defined(PARLATYPE_COMPILATION)
#error "Only <parlatype.h> can be included directly."
#endif

#include "pt-config.h"
#include "pt-media-info.h"
#include "pt-waveviewer.h"

G_BEGIN_DECLS

#define PT_TYPE_PLAYER (pt_player_get_type ())
G_DECLARE_DERIVABLE_TYPE (PtPlayer, pt_player, PT, PLAYER, GObject)

struct _PtPlayerClass
{
  GObjectClass parent_class;
};

/**
 * PtStateType:
 * @PT_STATE_STOPPED: the player is stopped
 * @PT_STATE_PAUSED: the player is paused
 * @PT_STATE_PLAYING: the player is currently playing a stream
 *
 * Enum values indicating PtPlayer’s current state.
 */
typedef enum
{
  PT_STATE_STOPPED,
  PT_STATE_PAUSED,
  PT_STATE_PLAYING,
  /*< private >*/
  PT_STATE_INVALID
} PtStateType;

/**
 * PtModeType:
 * @PT_MODE_PLAYBACK: normal audible playback
 * @PT_MODE_ASR: silent automatic speech recognition
 *
 * Enum values indicating PtPlayer output mode.
 */
typedef enum
{
  PT_MODE_PLAYBACK,
  PT_MODE_ASR,
  /*< private >*/
  PT_MODE_INVALID
} PtModeType;

/**
 * PtPrecisionType:
 * @PT_PRECISION_SECOND: Rounds to full seconds, e.g. 1:23 (1 minute, 23 seconds)
 * @PT_PRECISION_SECOND_10TH: Round to 1/10 seconds, e.g. 1:23.4
 * @PT_PRECISION_SECOND_100TH: Round to 1/100 seconds, e.g. 1:23.45
 *
 * Enum values indicating desired precision of time strings.
 */
typedef enum
{
  PT_PRECISION_SECOND,
  PT_PRECISION_SECOND_10TH,
  PT_PRECISION_SECOND_100TH,
  /*< private >*/
  PT_PRECISION_INVALID
} PtPrecisionType;


void       pt_player_pause                    (PtPlayer       *self);

void       pt_player_pause_and_rewind         (PtPlayer       *self);

gint       pt_player_get_pause                (PtPlayer       *self);

void       pt_player_play                     (PtPlayer       *self);

void       pt_player_play_pause               (PtPlayer       *self);

void       pt_player_set_selection            (PtPlayer       *self,
                                               gint64          start,
                                               gint64          end);

void       pt_player_clear_selection          (PtPlayer       *self);

gboolean   pt_player_has_selection            (PtPlayer       *self);

gboolean   pt_player_open_uri                 (PtPlayer       *self,
                                               gchar          *uri);

void       pt_player_jump_relative            (PtPlayer       *self,
                                               gint            milliseconds);

void       pt_player_jump_back                (PtPlayer       *self);

void       pt_player_jump_forward             (PtPlayer       *self);

gint       pt_player_get_back                 (PtPlayer       *self);

gint       pt_player_get_forward              (PtPlayer       *self);

void       pt_player_jump_to_position         (PtPlayer       *self,
                                               gint            milliseconds);

gdouble    pt_player_get_speed                (PtPlayer       *self);

void       pt_player_set_speed                (PtPlayer       *self,
                                               gdouble         speed);

gdouble    pt_player_get_volume               (PtPlayer       *self);

void       pt_player_set_volume               (PtPlayer       *self,
                                               gdouble         volume);

gboolean   pt_player_get_mute                 (PtPlayer       *self);

void       pt_player_set_mute                 (PtPlayer       *self,
                                               gboolean        mute);

gint64     pt_player_get_position             (PtPlayer       *self);

gint64     pt_player_get_duration             (PtPlayer       *self);

gchar     *pt_player_get_uri                  (PtPlayer       *self);

gchar     *pt_player_get_filename             (PtPlayer       *self);

gchar     *pt_player_get_time_string          (gint            time,
                                               gint            duration,
                                               PtPrecisionType precision);

gchar     *pt_player_get_current_time_string  (PtPlayer       *self,
                                               PtPrecisionType precision);

gchar     *pt_player_get_duration_time_string (PtPlayer       *self,
                                               PtPrecisionType precision);

gchar     *pt_player_get_timestamp_for_time   (PtPlayer       *self,
                                               gint            time,
                                               gint            duration);

gchar     *pt_player_get_timestamp            (PtPlayer       *self);

gint       pt_player_get_timestamp_position   (PtPlayer       *self,
                                               gchar          *timestamp,
                                               gboolean        check_duration);

gboolean   pt_player_string_is_timestamp      (PtPlayer       *self,
                                               gchar          *timestamp,
                                               gboolean        check_duration);

gboolean   pt_player_goto_timestamp           (PtPlayer       *self,
                                               gchar          *timestamp);

void       pt_player_connect_waveviewer       (PtPlayer       *self,
                                               PtWaveviewer   *wv);

void       pt_player_set_mode                 (PtPlayer       *self,
                                               PtModeType      type);

PtModeType pt_player_get_mode                 (PtPlayer       *self);

gboolean   pt_player_configure_asr            (PtPlayer       *self,
                                               PtConfig       *config,
                                               GError        **error);

gboolean   pt_player_config_is_loadable       (PtPlayer       *self,
                                               PtConfig       *config);

PtMediaInfo *pt_player_get_media_info         (PtPlayer       *self);

PtPlayer  *pt_player_new                      (void);

G_END_DECLS
