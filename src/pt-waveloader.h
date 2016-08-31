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


#ifndef PT_WAVELOADER_H
#define PT_WAVELOADER_H

#include "config.h"
#include <gio/gio.h>

#define PT_WAVELOADER_TYPE		(pt_waveloader_get_type())
#define PT_WAVELOADER(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_WAVELOADER_TYPE, PtWaveloader))
#define PT_IS_WAVELOADER(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_WAVELOADER_TYPE))

typedef struct _PtWaveloader		PtWaveloader;
typedef struct _PtWaveloaderClass	PtWaveloaderClass;
typedef struct _PtWaveloaderPrivate	PtWaveloaderPrivate;

struct _PtWaveloader
{
	GObject parent;

	/*< private > */
	PtWaveloaderPrivate *priv;
};

struct _PtWaveloaderClass
{
	GObjectClass parent_class;
};

GType		pt_waveloader_get_type		(void) G_GNUC_CONST;

gboolean	pt_waveloader_load_finish	(PtWaveloader  *wl,
						 GAsyncResult  *result,
						 GError       **error);

void		pt_waveloader_load_async	(PtWaveloader	     *wl,
						 GCancellable	     *cancellable,
						 GAsyncReadyCallback  callback,
						 gpointer	      user_data);

gint64		pt_waveloader_get_duration	(PtWaveloader *wl);

gint		pt_waveloader_get_channels	(PtWaveloader *wl);

gint		pt_waveloader_get_px_per_sec	(PtWaveloader *wl);

gint64		pt_waveloader_get_data_size	(PtWaveloader *wl);


gfloat		*pt_waveloader_get_data		(PtWaveloader *wl);

PtWaveloader	*pt_waveloader_new		(gchar *uri);

#endif
