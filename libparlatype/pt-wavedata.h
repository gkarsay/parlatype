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


#ifndef PT_WAVEDATA_H
#define PT_WAVEDATA_H

#include <glib-object.h>
#include <glib.h>

#define PT_TYPE_WAVEDATA		(pt_wavedata_get_type ())

typedef struct _PtWavedata PtWavedata;

/**
 * PtWavedata:
 * @array: (array length=length): array of min/max values
 * @length: number of elements in array
 * @channels: number of channels for future use
 * @px_per_sec: pixels/samples per second
 *
 * Contains all information about the wave.
 */
struct _PtWavedata
{
	gfloat *array;
	gint64  length;
	guint   channels;
	guint   px_per_sec;
};

GType 		pt_wavedata_get_type	(void) G_GNUC_CONST;
PtWavedata*	pt_wavedata_copy	(PtWavedata *data);
void		pt_wavedata_free	(PtWavedata *data);
PtWavedata*	pt_wavedata_new		(gfloat *array,
					 gint64  length,
					 guint   channels,
					 guint   px_per_sec);

#endif
