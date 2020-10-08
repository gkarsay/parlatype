/* Copyright (C) Gabor Karsay 2016-2018 <gabor.karsay@gmx.at>
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
#include "pt-waveviewer.h"

G_BEGIN_DECLS

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
	GObject *sphinx;

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

void		pt_player_pause			(PtPlayer *player);
void		pt_player_pause_and_rewind	(PtPlayer *player);
gint		pt_player_get_pause		(PtPlayer *player);
void		pt_player_play			(PtPlayer *player);
void		pt_player_play_pause		(PtPlayer *player);
void		pt_player_set_selection		(PtPlayer *player,
					         gint64    start,
					         gint64    end);
void		pt_player_clear_selection	(PtPlayer *player);
gboolean	pt_player_selection_active	(PtPlayer *player);
gboolean	pt_player_open_uri		(PtPlayer *player,
						 gchar    *uri);
void		pt_player_jump_relative		(PtPlayer *player,
						 gint      milliseconds);
void		pt_player_jump_back		(PtPlayer *player);
void		pt_player_jump_forward		(PtPlayer *player);
gint		pt_player_get_back		(PtPlayer *player);
gint		pt_player_get_forward		(PtPlayer *player);
void		pt_player_jump_to_position	(PtPlayer *player,
						 gint      milliseconds);
void		pt_player_jump_to_permille	(PtPlayer *player,
						 guint     permille);
gint		pt_player_get_permille		(PtPlayer *player);
void		pt_player_set_speed		(PtPlayer *player,
						 gdouble   speed);
gdouble		pt_player_get_volume		(PtPlayer *player);
void		pt_player_set_volume		(PtPlayer *player,
						 gdouble   volume);
gboolean	pt_player_get_mute		(PtPlayer *player);
void		pt_player_set_mute		(PtPlayer *player,
						 gboolean  mute);
#ifndef PT_DISABLE_DEPRECATED
G_DEPRECATED_FOR(pt_player_set_mute)
void		pt_player_mute_volume		(PtPlayer *player,
						 gboolean  mute);
#endif
void		pt_player_rewind		(PtPlayer *player,
						 gdouble   speed);
void		pt_player_fast_forward		(PtPlayer *player,
						 gdouble   speed);
gint64		pt_player_get_position		(PtPlayer *player);
gint64		pt_player_get_duration		(PtPlayer *player);
gchar*		pt_player_get_uri		(PtPlayer *player);
gchar*		pt_player_get_filename		(PtPlayer *player);
gchar*		pt_player_get_time_string	(gint  time,
						 gint  duration,
						 PtPrecisionType precision);
gchar*		pt_player_get_current_time_string	(PtPlayer *player,
							 PtPrecisionType precision);
gchar*		pt_player_get_duration_time_string	(PtPlayer *player,
							 PtPrecisionType precision);
gchar*		pt_player_get_timestamp_for_time	(PtPlayer *player,
			                                 gint      time,
			                                 gint      duration);
gchar*		pt_player_get_timestamp		(PtPlayer *player);
gint		pt_player_get_timestamp_position	(PtPlayer *player,
							 gchar    *timestamp,
							 gboolean  check_duration);
gboolean	pt_player_string_is_timestamp	(PtPlayer *player,
						 gchar    *timestamp,
						 gboolean  check_duration);
gboolean	pt_player_goto_timestamp	(PtPlayer *player,
						 gchar    *timestamp);
void		pt_player_connect_waveviewer	(PtPlayer *player,
						 PtWaveviewer *wv);
gboolean	pt_player_setup_player		(PtPlayer  *player,
			                         GError   **error);
gboolean	pt_player_setup_sphinx		(PtPlayer  *player,
			                         GError   **error);
PtPlayer*	pt_player_new			(void);

G_END_DECLS

#endif
