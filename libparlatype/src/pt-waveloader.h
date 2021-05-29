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

#if !defined (__PARLATYPE_H_INSIDE__) && !defined (PARLATYPE_COMPILATION)
#error "Only <parlatype.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVELOADER		(pt_waveloader_get_type())
#define PT_WAVELOADER(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_WAVELOADER, PtWaveloader))
#define PT_IS_WAVELOADER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_WAVELOADER))

typedef struct _PtWaveloader		PtWaveloader;
typedef struct _PtWaveloaderClass	PtWaveloaderClass;
typedef struct _PtWaveloaderPrivate	PtWaveloaderPrivate;


/**
 * PtWaveloader:
 *
 * The #PtWaveloader contains only private fields and should not be directly accessed.
 */
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
						 gint                 pps,
						 GCancellable	     *cancellable,
						 GAsyncReadyCallback  callback,
						 gpointer	      user_data);

gint64		pt_waveloader_get_duration	(PtWaveloader *wl);

gboolean	pt_waveloader_resize_finish	(PtWaveloader  *wl,
						 GAsyncResult  *result,
						 GError       **error);

void		pt_waveloader_resize_async	(PtWaveloader        *wl,
						 gint                 pps,
						 GCancellable        *cancellable,
						 GAsyncReadyCallback  callback,
						 gpointer             user_data);

gboolean	pt_waveloader_resize		(PtWaveloader *wl,
						 gint          pps,
						 GError      **error);

GArray		*pt_waveloader_get_data		(PtWaveloader *wl);

PtWaveloader*	pt_waveloader_new		(gchar *uri);

G_END_DECLS

#endif
