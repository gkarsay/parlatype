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


#include <string.h>
#include "pt-wavedata.h"


/**
 * SECTION: pt-wavedata
 * @short_description: A boxed type with wave data.
 * @stability: Unstable
 * @include: parlatype/pt-wavedata.h
 *
 * Contains all information needed to show a wave form. It's produced by
 * #PtPlayer or #PtWaveloader. Pass it to #PtWaveslider to visualize the wave.
 * #PtWaveslider copies the data, you can free it immediately with pt_wavedata_free().
 *
 * There is no need to access the struct members.
 *
 * Assuming you have set up a PtPlayer *player, typical usage would be:
 * |[<!-- language="C" -->
 * ...
 * PtWavedata *data;
 * data = pt_player_get_data (player);
 * pt_waveslider_set_wave (PT_WAVESLIDER (waveslider), data);
 * pt_wavedata_free (data);
 * ...
 * ]|
 */

G_DEFINE_BOXED_TYPE (PtWavedata, pt_wavedata, pt_wavedata_copy, pt_wavedata_free);

/**
 * pt_wavedata_copy: (skip)
 * @data: the object to copy
 *
 * Creates a copy of @data.
 *
 * Return value: a new #PtWavedata, free after use with pt_wavedata_free()
 */
PtWavedata*
pt_wavedata_copy (PtWavedata *data)
{
	PtWavedata *new;
	new = g_new0 (PtWavedata, 1);
	
	if (!(new->array = g_try_malloc (sizeof (gfloat) * data->length))) {
		g_debug	("copying wavedata failed");
		return data;
	}

	memcpy (new->array, data->array, sizeof (gfloat) * new->length);
	new->length = data->length;
	new->channels = data->channels;
	new->px_per_sec = data->px_per_sec;

	return new;
}

/**
 * pt_wavedata_free: (skip)
 * @data: the object to be freed
 *
 * Free's @data.
 */
void pt_wavedata_free (PtWavedata *data)
{
	g_debug ("pt wavedata free");

	if (!data)
		return;

	g_free (data->array);
	g_free (data);
}

/**
 * pt_wavedata_new: (constructor)
 * @array: (array length=length): an array of floats
 * @length: number of elements in array
 * @channels: number of channels
 * @px_per_sec: rate
 *
 * Constructs a new #PtWavedata.
 *
 * Returns: (transfer full): new data, free after use with pt_wavedata_free()
 */
PtWavedata*
pt_wavedata_new (gfloat *array,
		 gint64  length,
		 guint   channels,
		 guint   px_per_sec)
{
	g_debug ("pt wavedata new");

	PtWavedata *data;
	data = g_new0 (PtWavedata, 1);

	if (!(data->array = g_try_malloc (sizeof (gfloat) * length))) {
		g_debug	("creating new wavedata failed");
		return data;
	}
	
	memcpy (data->array, array, sizeof(gfloat) * length);
	data->length = length;
	data->channels = channels;
	data->px_per_sec = px_per_sec;

	return data;
}
