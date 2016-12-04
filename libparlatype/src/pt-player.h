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


#ifndef PT_PLAYER_H
#define PT_PLAYER_H

#include <gio/gio.h>
#include "pt-wavedata.h"

#define PT_TYPE_PLAYER		(pt_player_get_type())
#define PT_PLAYER(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_PLAYER, PtPlayer))
#define PT_IS_PLAYER(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_PLAYER))

typedef struct _PtPlayer	PtPlayer;
typedef struct _PtPlayerClass	PtPlayerClass;
typedef struct _PtPlayerPrivate PtPlayerPrivate;

/**
 * PtPlayer:
 *
 * The #PtPlayer contains only private fields and should not be directly accessed.
 */
struct _PtPlayer
{
	GObject parent;

	/*< private > */
	PtPlayerPrivate *priv;
};

struct _PtPlayerClass
{
	GObjectClass parent_class;
};


/**
 * PtPrecisionType:
 * @PT_PRECISION_SECOND: Rounds to full seconds, e.g. 1:23 (1 minute, 23 seconds)
 * @PT_PRECISION_SECOND_10TH: Round to 1/10 seconds, e.g. 1:23.4
 * @PT_PRECISION_SECOND_100TH: Round to 1/100 seconds, e.g. 1:23.45
 *
 * Enum values indicating desired precision of time strings.
 */
typedef enum {
	PT_PRECISION_SECOND,
	PT_PRECISION_SECOND_10TH,
	PT_PRECISION_SECOND_100TH,
	/*< private >*/
	PT_PRECISION_INVALID
} PtPrecisionType;


GType		pt_player_get_type		(void) G_GNUC_CONST;

gint64		pt_player_wave_pos		(PtPlayer *player);
PtWavedata*	pt_player_get_data		(PtPlayer *player,
						 gint      pps);
void		pt_player_pause			(PtPlayer *player);
void		pt_player_play			(PtPlayer *player);
void		pt_player_set_selection		(PtPlayer *player,
					         gint64    start,
					         gint64    end);
void		pt_player_clear_selection	(PtPlayer *player);
gboolean	pt_player_open_uri_finish	(PtPlayer      *player,
						 GAsyncResult  *result,
						 GError       **error);
void		pt_player_open_uri_async	(PtPlayer	     *player,
						 gchar		     *uri,
						 GAsyncReadyCallback  callback,
						 gpointer	      user_data);
gboolean	pt_player_open_uri		(PtPlayer *player,
						 gchar    *uri,
						 GError  **error);
void		pt_player_cancel		(PtPlayer *player);
void		pt_player_jump_relative		(PtPlayer *player,
						 gint      milliseconds);
void		pt_player_jump_to_position	(PtPlayer *player,
						 gint      milliseconds);
void		pt_player_jump_to_permille	(PtPlayer *player,
						 guint     permille);
gint		pt_player_get_permille		(PtPlayer *player);
void		pt_player_set_speed		(PtPlayer *player,
						 gdouble   speed);
void		pt_player_set_volume		(PtPlayer *player,
						 gdouble   volume);
void		pt_player_mute_volume		(PtPlayer *player,
						 gboolean  mute);
void		pt_player_rewind		(PtPlayer *player,
						 gdouble   speed);
void		pt_player_fast_forward		(PtPlayer *player,
						 gdouble   speed);
gint		pt_player_get_position		(PtPlayer *player);
gint		pt_player_get_duration		(PtPlayer *player);
gchar*		pt_player_get_uri		(PtPlayer *player);
gchar*		pt_player_get_filename		(PtPlayer *player);
gchar*		pt_player_get_time_string	(gint  time,
						 gint  duration,
						 PtPrecisionType precision);
gchar*		pt_player_get_current_time_string	(PtPlayer *player,
							 PtPrecisionType precision);
gchar*		pt_player_get_duration_time_string	(PtPlayer *player,
							 PtPrecisionType precision);
gchar*		pt_player_get_timestamp		(PtPlayer *player);
gboolean	pt_player_string_is_timestamp	(PtPlayer *player,
						 gchar    *timestamp);
gboolean	pt_player_goto_timestamp	(PtPlayer *player,
						 gchar    *timestamp);

PtPlayer*	pt_player_new			(GError **error);

#endif
