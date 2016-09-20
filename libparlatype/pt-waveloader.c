/* Copyright (C) 2006 Buzztrax team <buzztrax-devel@buzztrax.org>
 * Copyright (C) Gabor Karsay 2016 <gabor.karsay@gmx.at>
 *
 * Taken from wave.c (bt_wave_load_from_uri) from Buzztrax and heavily modified.
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


#define _POSIX_SOURCE

#include "config.h"
#include <stdio.h>	/* sscanf */
#include <gio/gio.h>
#include <glib/gi18n.h>	
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <sys/stat.h>	/* fstat */
#include "pt-waveloader.h"

struct _PtWaveloaderPrivate
{
	GstElement *pipeline;
	GstElement *fmt;

	gchar      *uri;
	gboolean    downmix;
	gboolean    pps;

	gint64	    duration;
	gint	    channels;
	gint	    rate;
	gint64	    data_size;

	gint	    bus_watch_id;
	gint	    progress_timeout;
	gdouble	    progress;

	gint	    fd;
	FILE	   *tf;
};

enum
{
	PROP_0,
	PROP_URI,
	PROP_DOWNMIX,
	PROP_PPS,
	N_PROPERTIES
};


static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (PtWaveloader, pt_waveloader, G_TYPE_OBJECT)


/**
 * SECTION: pt-waveloader
 * @short_description: Loads the waveform for a given file.
 * @include: parlatype-1.0/pt-waveloader.h
 *
 * An object to load waveform data from an audio file. The raw data can be
 * used by a widget to visually represent a waveform.
 */

static void
remove_timeout (PtWaveloader *wl)
{
	if (wl->priv->progress_timeout > 0) {
		g_source_remove (wl->priv->progress_timeout);
		wl->priv->progress_timeout = 0;
	}
}


static void
on_wave_loader_new_pad (GstElement *bin,
			GstPad	   *pad,
			gpointer    user_data)
{
	if (!gst_element_link (bin, GST_ELEMENT (user_data))) {
		GST_WARNING ("Can't link output of wave decoder to converter.");
	}
}

static gboolean
setup_pipeline (PtWaveloader *wl)
{
	gboolean result = TRUE;
	GstElement *src, *dec, *conv, *sink;
	GstCaps *caps;

	/* create loader pipeline */
	wl->priv->pipeline = gst_pipeline_new ("wave-loader");
	src 		   = gst_element_make_from_uri (GST_URI_SRC, wl->priv->uri, NULL, NULL);
	dec 		   = gst_element_factory_make ("decodebin", NULL);
	conv 		   = gst_element_factory_make ("audioconvert", NULL);
	wl->priv->fmt 	   = gst_element_factory_make ("capsfilter", NULL);
	sink 		   = gst_element_factory_make ("fdsink", NULL);

	/* configure elements */
	caps = gst_caps_new_simple ("audio/x-raw",
				    "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
				    "layout", G_TYPE_STRING, "interleaved",
				    "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);

	if (wl->priv->downmix)
		gst_caps_set_simple (caps, "channels", G_TYPE_INT, 1, NULL);
	else
		gst_caps_set_simple (caps, "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);

	g_object_set (wl->priv->fmt, "caps", caps, NULL);
	gst_caps_unref (caps);

	g_object_set (sink, "fd", wl->priv->fd, "sync", FALSE, NULL);

	/* add and link */
	gst_bin_add_many (GST_BIN (wl->priv->pipeline), src, dec, conv, wl->priv->fmt, sink, NULL);
	result = gst_element_link (src, dec);
	if (!result) {
		GST_WARNING_OBJECT (wl->priv->pipeline,
			"Can't link wave loader pipeline (src ! dec ! conv ! fmt ! sink).");
		return result;
	}

	result = gst_element_link_many (conv, wl->priv->fmt, sink, NULL);
	if (!result) {
		GST_WARNING_OBJECT (wl->priv->pipeline,
			"Can't link wave loader pipeline (conf ! fmt ! sink).");
		return result;
	}

	g_signal_connect (dec, "pad-added", G_CALLBACK (on_wave_loader_new_pad),
			(gpointer) conv);

	return result;
}

static gboolean
check_progress (GTask *task)
{
	/* 1) Query position and emit progress signal
	   2) Check if task was cancelled and reset pipeline */

	PtWaveloader *wl = g_task_get_source_object (task);

	gint64  dur;
	gint64  pos;
	gdouble temp;

	if (g_cancellable_is_cancelled (g_task_get_cancellable (task))) {
		gst_element_set_state (wl->priv->pipeline, GST_STATE_NULL);
		g_source_remove (wl->priv->bus_watch_id);
		wl->priv->bus_watch_id = 0;
		wl->priv->progress_timeout = 0;
		g_task_return_boolean (task, FALSE);
		return FALSE;
	}

	if (!gst_element_query_position (wl->priv->pipeline, GST_FORMAT_TIME, &pos))
		return TRUE;

	if (!gst_element_query_duration (wl->priv->pipeline, GST_FORMAT_TIME, &dur))
		return TRUE;

	temp = (gdouble) pos / dur;

	if (temp > wl->priv->progress && temp < 1) {
		wl->priv->progress = temp;
		g_signal_emit_by_name (wl, "progress", wl->priv->progress);
	}

	return TRUE;
}

static gboolean
bus_handler (GstBus     *bus,
	     GstMessage *msg,
	     gpointer    data)
{
	GTask *task = (GTask *) data;
	PtWaveloader *wl = g_task_get_source_object (task);

	g_debug ("Message: %s; sent by: %s", GST_MESSAGE_TYPE_NAME (msg), GST_MESSAGE_SRC_NAME (msg));

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ERROR: {
		gchar    *debug;
		GError   *error;

		remove_timeout (wl);
		gst_message_parse_error (msg, &error, &debug);
		g_debug ("ERROR from element %s: %s", GST_OBJECT_NAME (msg->src), error->message);
		g_debug ("Debugging info: %s", (debug) ? debug : "none");
		g_free (debug);
		wl->priv->bus_watch_id = 0;
		g_task_return_error (task, error);
		return FALSE;
		}

	case GST_MESSAGE_EOS: {
		GstPad *pad;
		/* query length and convert to samples */
		if (!gst_element_query_duration (wl->priv->pipeline, GST_FORMAT_TIME, &wl->priv->duration)) {
			GST_WARNING ("getting sample duration failed");
		}

		/* get caps for sample rate and channels */
		if ((pad = gst_element_get_static_pad (wl->priv->fmt, "src"))) {
			GstCaps *caps = gst_pad_get_current_caps (pad);
			if (caps && GST_CAPS_IS_SIMPLE (caps)) {
				GstStructure *structure = gst_caps_get_structure (caps, 0);

				gst_structure_get_int (structure, "channels", &wl->priv->channels);
				gst_structure_get_int (structure, "rate", &wl->priv->rate);

			} else {
				GST_WARNING ("No caps or format has not been fixed.");
				wl->priv->channels = 1;
				wl->priv->rate = GST_AUDIO_DEF_RATE;
			}
			if (caps)
				gst_caps_unref (caps);
			gst_object_unref (pad);
		}

		g_debug ("sample decoded: channels=%d, rate=%d, length=%" GST_TIME_FORMAT,
			wl->priv->channels, wl->priv->rate, GST_TIME_ARGS (wl->priv->duration));

		/* stat temp file, query size in bytes and compute number of samples */
		struct stat buf;

		if (fstat (wl->priv->fd, &buf) != 0) {
			g_task_return_new_error (
					task,
					G_FILE_ERROR,
					G_FILE_ERROR_FAILED,
					_("Failed to open temporary file."));

			remove_timeout (wl);
			return FALSE;
		}

		/* Adjust pixel per second ratio if there's a remainder */
		gint i;
		for (i = wl->priv->pps; i > 10; i--) {
			if (wl->priv->rate % i == 0) {
				wl->priv->pps = i;
				break;
			}
		}

		gint chunk_size = wl->priv->rate / wl->priv->pps;
		wl->priv->data_size = buf.st_size / chunk_size;

		g_debug ("pixels per sec: %d", wl->priv->pps);
		g_debug ("samples: %" G_GINT64_FORMAT " ", wl->priv->data_size);

		remove_timeout (wl);
		g_task_return_boolean (task, TRUE);
		return FALSE;
		}
	case GST_MESSAGE_DURATION_CHANGED:
		break;
	default:
		break;
	}

	return TRUE;
}

/**
 * pt_waveloader_load_finish:
 * @wl: a #PtWaveloader
 * @result: the #GAsyncResult passed to your #GAsyncReadyCallback
 * @error: a pointer to a NULL #GError, or NULL
 *
 * Gives the result of the async load operation. A cancelled operation results
 * in an error, too.
 *
 * Return value: TRUE if successful, or FALSE with error set
 */
gboolean
pt_waveloader_load_finish (PtWaveloader  *wl,
			   GAsyncResult  *result,
			   GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (result, wl), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * pt_waveloader_load_async:
 * @wl: a #PtWaveloader
 * @cancellable: a #GCancellable or NULL
 * @callback: a #GAsyncReadyCallback to call when the operation is complete
 * @user_data: user_data for callback
 *
 * Writes the raw sample data to a temporary file and also gets the number of
 * channels, the bit rate and the exact duration. Load only once and on success
 * the data and information can be retrieved. It's a programmer's error trying
 * to retrieve the data without prior loading.
 *
 * Emits a progress signal while saving the temporary file.
 *
 * In your callback call #pt_waveloader_load_finish to get the result of the
 * operation.
 */
void
pt_waveloader_load_async (PtWaveloader	       *wl,
			  GCancellable	       *cancellable,
			  GAsyncReadyCallback   callback,
			  gpointer		user_data)
{
	GTask  *task;
	GstBus *bus;

	task = g_task_new (wl, cancellable, callback, user_data);

	/* setup file descriptor */
	if (!(wl->priv->tf = tmpfile ())) {
		g_task_return_new_error (
				task,
				G_FILE_ERROR,
				G_FILE_ERROR_FAILED,
				_("Failed to open temporary file."));

		g_object_unref (task);
		return;
	}
	wl->priv->fd = fileno (wl->priv->tf);

	/* setup pipeline */
	if (!setup_pipeline (wl)) {
		g_task_return_new_error (
				task,
				GST_CORE_ERROR,
				GST_CORE_ERROR_FAILED,
				_("Failed to setup GStreamer pipeline."));

		g_object_unref (task);
		return;
	}

	/* setup message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (wl->priv->pipeline));
	wl->priv->bus_watch_id = gst_bus_add_watch (bus, bus_handler, task);
	gst_object_unref (bus);

	/* run pipeline and start progress (and cancel) timeout */
	if (gst_element_set_state (wl->priv->pipeline,
			GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		gst_element_set_state (wl->priv->pipeline, GST_STATE_NULL);
		g_task_return_new_error (
				task,
				GST_CORE_ERROR,
				GST_CORE_ERROR_STATE_CHANGE,
				_("Failed to start GStreamer pipeline."));

		g_object_unref (task);
		return;
	}

	wl->priv->progress_timeout = g_timeout_add (30, (GSourceFunc) check_progress, task);
}

/**
 * pt_waveloader_get_duration:
 * @wl: a #PtWaveloader
 *
 * Returns the duration of stream. As the whole stream is scanned, this is
 * supposed to be an exact duration, not an estimate.
 *
 * Return value: duration in nanoseconds as used by GStreamer
 */
gint64
pt_waveloader_get_duration (PtWaveloader *wl)
{
	return wl->priv->duration;
}

/**
 * pt_waveloader_get_channels:
 * @wl: a #PtWaveloader
 *
 * Get the number of channels for the stream. We accept only mono (1 channel)
 * or stereo (2 channels). As of now, only mono is implemented.
 *
 * Return value: number of channels (1 or 2)
 */
gint
pt_waveloader_get_channels (PtWaveloader *wl)
{
	return wl->priv->channels;
}

/**
 * pt_waveloader_get_px_per_sec:
 * @wl: a #PtWaveloader
 *
 * Returns the bit rate or samples/pixels per second.
 *
 * Return value: pixels per second
 */
gint
pt_waveloader_get_px_per_sec (PtWaveloader *wl)
{
	return wl->priv->pps;
}

/**
 * pt_waveloader_get_data_size:
 * @wl: a #PtWaveloader
 *
 * Returns the number of elements in the data array. This is twice the number
 * of samples, as each sample consists of 2 elements (min and max value).
 *
 * Return value: number of elements
 */
gint64
pt_waveloader_get_data_size (PtWaveloader *wl)
{
	return wl->priv->data_size;
}

/**
 * pt_waveloader_get_data:
 * @wl: a #PtWaveloader
 *
 * Returns all samples for visual representation as raw data. Each sample
 * contains a minimum followed by a maximum peak value. Minimum values range
 * from -1.0 to 0, maximum values from 0 to 1.0.
 *
 * The raw data is only useful with additional information, first of all the
 * number of elements in the array, the number of channels (as for now it's
 * always mono) and the pixel per second ratio.
 *
 * Return value: an array of all samples
 */
gfloat *
pt_waveloader_get_data (PtWaveloader *wl)
{
	gfloat *data = NULL;
	gint64 i;
	gint k;
	gfloat d, vmin, vmax;

	gint chunk_size;
	gint chunk_bytes;

	chunk_size = wl->priv->rate / wl->priv->pps;
	chunk_bytes = 2 * chunk_size;

	gint16 temp[chunk_size];

	if (!(data = g_try_malloc (sizeof (gfloat) * wl->priv->data_size * 2))) {
		g_debug	("sample is too long or empty");
		return NULL;
	}

	if (lseek (wl->priv->fd, 0, SEEK_SET) != 0) {
		g_debug ("sample not loaded!!!");
		return NULL;
	}

	ssize_t bytes __attribute__ ((unused));
	for (i = 0; i <  wl->priv->data_size; i++) {
		bytes = read (wl->priv->fd, temp, chunk_bytes);
		vmin = 0;
		vmax = 0;
		for (k = 0; k < chunk_size; k++) {
			d = temp[k];
			if (d < vmin)
				vmin = d;
			if (d > vmax)
				vmax = d;
		}
		if (vmin > 0 && vmax > 0)
			vmin = 0;
		else if (vmin < 0 && vmax < 0)
			vmax = 0;
		data[i*2] = vmin / 32768.0;
		data[i*2+1] = vmax / 32768.0;
	}

	return data;
}


/* --------------------- Init and GObject management ------------------------ */

static void
pt_waveloader_init (PtWaveloader *wl)
{
	wl->priv = pt_waveloader_get_instance_private (wl);

	wl->priv->pipeline = NULL;
	wl->priv->fd = -1;
	wl->priv->progress = 0;
	wl->priv->bus_watch_id = 0;
	wl->priv->progress_timeout = 0;
	wl->priv->data_size = 0;
}

static void
pt_waveloader_dispose (GObject *object)
{
	PtWaveloader *wl;
	wl = PT_WAVELOADER (object);

	g_free (wl->priv->uri);	

	if (wl->priv->tf) {
		// fd is fileno() of a tf
		fclose (wl->priv->tf);
		wl->priv->tf = NULL;
		wl->priv->fd = -1;
	} else if (wl->priv->fd != -1) {
		close (wl->priv->fd);
		wl->priv->fd = -1;
	}

	if (wl->priv->bus_watch_id > 0) {
		g_source_remove (wl->priv->bus_watch_id);
		wl->priv->bus_watch_id = 0;
	}

	remove_timeout (wl);

	if (wl->priv->pipeline) {
		
		gst_element_set_state (wl->priv->pipeline, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (wl->priv->pipeline));
		wl->priv->pipeline = NULL;
	}

	G_OBJECT_CLASS (pt_waveloader_parent_class)->dispose (object);
}

static void
pt_waveloader_set_property (GObject      *object,
			    guint         property_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	PtWaveloader *wl;
	wl = PT_WAVELOADER (object);

	switch (property_id) {
	case PROP_URI:
		g_free (wl->priv->uri);
		wl->priv->uri = g_value_dup_string (value);
		break;
	case PROP_DOWNMIX:
		wl->priv->downmix = g_value_get_boolean (value);
		break;
	case PROP_PPS:
		wl->priv->pps = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveloader_get_property (GObject    *object,
			    guint       property_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	PtWaveloader *wl;
	wl = PT_WAVELOADER (object);

	switch (property_id) {
	case PROP_URI:
		g_value_set_string (value, wl->priv->uri);
		break;
	case PROP_DOWNMIX:
		g_value_set_boolean (value, wl->priv->downmix);
		break;
	case PROP_PPS:
		g_value_set_int (value, wl->priv->pps);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveloader_class_init (PtWaveloaderClass *klass)
{
	G_OBJECT_CLASS (klass)->set_property = pt_waveloader_set_property;
	G_OBJECT_CLASS (klass)->get_property = pt_waveloader_get_property;
	G_OBJECT_CLASS (klass)->dispose = pt_waveloader_dispose;

	/**
	* PtWaveloader::progress:
	* @wl: the waveloader emitting the signal
	* @progress: the new progress state, ranging from 0.0 to 1.0
	*
	* Indicates progress on a scale from 0.0 to 1.0, however it does not
	* emit the value 0.0 nor 1.0. Wait for a successful operation until
	* any gui element showing progress is dismissed.
	*/
	g_signal_new ("progress",
		      PT_TYPE_WAVELOADER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__DOUBLE,
		      G_TYPE_NONE,
		      1, G_TYPE_DOUBLE);

	/**
	* PtWaveloader:uri:
	*
	* URI to load from.
	*/
	obj_properties[PROP_URI] =
	g_param_spec_string (
			"uri",
			"URI to load from",
			"URI to load from",
			"",
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	/**
	* PtWaveloader:downmix:
	*
	* Whether to downmix stream to mono.
	* This is not fully implemented yet, it MUST be mono for now.
	*/
	obj_properties[PROP_DOWNMIX] =
	g_param_spec_boolean (
			"downmix",
			"Downmix to mono",
			"Downmix to mono",
			TRUE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	/**
	* PtWaveloader:pixels_per_sec:
	*
	* Set it to the requested resolution in pixels per second.
	* Parlatype might not be able to use exactly that value and will
	* adjust it. Always re-check this value after loading a file.
	*/
	obj_properties[PROP_PPS] =
	g_param_spec_int (
			"pixels_per_sec",
			"Pixels per second",
			"Pixels per second",
			10,	/* min */
			1000,	/* max */
			100,	/* default */
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

/**
 * pt_waveloader_new:
 * @uri: URI of the audio file to load
 *
 * Returns a new #PtWaveloader.
 *
 * After use g_object_unref() it.
 *
 * Return value: (transfer full): a new #PtWaveloader
 */
PtWaveloader *
pt_waveloader_new (gchar *uri)
{
	return g_object_new (PT_TYPE_WAVELOADER,
			     "uri", uri,
			     NULL);
}
