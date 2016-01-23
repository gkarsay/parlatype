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

/* PtPlayer is the GStreamer backend for Parlatype.

   1. Call pt_player_new(). This is a failable constructor. It fails, if
      GStreamer doesn't init or a plugin is missing. In this case NULL is returned,
      error is set.
   2. Call pt_player_open_uri(). Works only with local files.
      Returns success as a boolean and an error if it fails.
      Only one file can be open at a time, playlists are not supported by the
      backend. Opening a new file will close the previous one.
      When closing a file or on object destruction PtPlayer tries to write the
      last position into the file's metadata.
      On opening a file it reads the metadata and jumps to the last known
      position if found.
   3. Play around with play, pause etc. Unlike pt_player_open_uri() these
      methods are not synchronous and might take a little while until they are done.
   4. Our time unit are milliseconds. For scale-widgets we use per mille (1/1000).
   5. While playing it emits these signals:
      - duration-changed: The duration is an estimate, that's why it can change
                          during playback.
      - end-of-stream: End of file reached, in the GUI you might want to jump
     		       to the beginning, reset play button etc.
      - error: A fatal error occured, the player is reset. There's an error message.
   6. Speed is a double from 0.5 to 1.5. 1.0 is normal playback, < 1.0 is slower,
      > 1.0 is faster. Changing the "speed" property doesn't change playback though.
      Use the method instead.
      Volume is a double from 0 to 1. It can be set via the method or setting
      the "volume" property.
   7. After use g_object_unref() it.
 */


#ifndef PT_PLAYER_H
#define PT_PLAYER_H

#include "config.h"
#include <gio/gio.h>

#define PT_PLAYER_TYPE		(pt_player_get_type())
#define PT_PLAYER(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_PLAYER_TYPE, PtPlayer))
#define PT_IS_PLAYER(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_PLAYER_TYPE))

typedef struct _PtPlayer	PtPlayer;
typedef struct _PtPlayerClass	PtPlayerClass;
typedef struct _PtPlayerPrivate PtPlayerPrivate;

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

GType		pt_player_get_type		(void) G_GNUC_CONST;

void		pt_player_pause			(PtPlayer *player);
void		pt_player_play			(PtPlayer *player);
gboolean	pt_player_open_uri		(PtPlayer  *player,
						 gchar     *uri,
						 GError   **error);
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
gchar*		pt_player_get_uri		(PtPlayer *player);
gchar*		pt_player_get_filename		(PtPlayer *player);
gchar*		pt_player_get_current_time_string	(PtPlayer *player,
							 guint     digits);
gchar*		pt_player_get_duration_time_string	(PtPlayer *player,
							 guint     digits);
gchar*		pt_player_get_timestamp		(PtPlayer *player);

PtPlayer	*pt_player_new			(gdouble   speed,
						 GError  **error);

#endif
