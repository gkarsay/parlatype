/* Copyright (C) 2006 Buzztrax team <buzztrax-devel@buzztrax.org>
 * Copyright (C) Gabor Karsay 2016, 2020 <gabor.karsay@gmx.at>
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


#include "config.h"
#include <gio/gio.h>
#define GETTEXT_PACKAGE "libparlatype"
#include <glib/gi18n-lib.h>
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/app/gstappsink.h>
#include "pt-i18n.h"
#include "pt-waveloader.h"

struct _PtWaveloaderPrivate
{
	GstElement *pipeline;
	GstElement *fmt;

	GArray     *hires;
	GArray     *lowres;
	gint        lowres_pps;
	gint        lowres_index;

	gchar      *uri;
	gboolean    load_pending;
	gboolean    data_pending;

	gint64      duration;

	gint        bus_watch_id;
	gint        progress_timeout;
	gdouble     progress;
};

enum
{
	PROP_0,
	PROP_URI,
	N_PROPERTIES
};

enum {
	PROGRESS,
	ARRAY_SIZE_CHANGED,
	LAST_SIGNAL
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (PtWaveloader, pt_waveloader, G_TYPE_OBJECT)


/**
 * SECTION: pt-waveloader
 * @short_description: Loads the waveform for a given file.
 * @stability: Stable
 * @include: parlatype/pt-waveloader.h
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
		GST_WARNING ("Can’t link output of wave decoder to converter.");
	}
}

static gint
calc_lowres_len (gint hires_len,
		 gint pps)
{
	/* High resolution array has 8000 samples (single values) per second.
	 * Convert to low resolution array with @pps × 2 values (min, max) per
	 * second. */

	gint i, ratio, mod, correct, remains, result;

	/* First full seconds */
	result = hires_len / 8000 * pps;

	/* Add remainder,
	 * keep in sync with implementation in convert_one_second() */
	remains = hires_len % 8000; /* what remains to convert */
	ratio = 8000 / pps;         /* conversion ratio */
	mod = 8000 % pps;	    /* remainder of ratio */

	for (i = 0; i < pps; i++) {
		result += 1;

		/* If there is a remainder for in_rate/out_rate, correct
		 * it by adding an additional sample. */
		correct = 0;
		if (i < mod)
			correct = 1;
		remains -= (ratio + correct);
		if (remains <= 0)
			break;
	}

	/* We have min/max pairs */
	result = result *2;

	return result;
}

static void
convert_one_second (PtWaveloader *wl,
		    gint          nr)
{
	gint pps = wl->priv->lowres_pps;
	gint k, m;
	gint16 d;
	gfloat vmin, vmax;
	gint chunk_size;
	gint mod;

	chunk_size = 8000 / pps;
	mod = 8000 % pps;

	gint index_in = nr * 8000;
	gint correct;
	if (index_in >= wl->priv->hires->len)
		return;

	/* Loop data worth 1 second */
	for (k = 0; k < pps; k++) {
		vmin = 0;
		vmax = 0;
		/* If there is a remainder for in_rate/out_rate, correct
		 * it by reading in an additional sample. */
		correct = 0;
		if (k < mod)
			correct = 1;
		for (m = 0; m < (chunk_size + correct); m++) {
			/* Get highest and lowest value */
			d = g_array_index (wl->priv->hires, gint16, index_in);
			if (d < vmin)
				vmin = d;
			if (d > vmax)
				vmax = d;
			index_in++;
			if (index_in == wl->priv->hires->len)
				break;
		}
		/* Always include 0, looks better at higher resolutions */
		if (vmin > 0 && vmax > 0)
			vmin = 0;
		else if (vmin < 0 && vmax < 0)
			vmax = 0;
		/* Save as a float in the range 0 to 1 */
		vmin = vmin / 32768.0;
		vmax = vmax / 32768.0;
		memcpy (wl->priv->lowres->data + wl->priv->lowres_index * sizeof (float), &vmin, sizeof (float));
		wl->priv->lowres_index++;
		memcpy (wl->priv->lowres->data + wl->priv->lowres_index * sizeof (float), &vmax, sizeof (float));
		wl->priv->lowres_index++;
		if (index_in == wl->priv->hires->len)
			break;
	}
}

static GstFlowReturn
new_sample_cb (GstAppSink *sink,
               gpointer    user_data)
{
	PtWaveloader *wl = PT_WAVELOADER (user_data);
	GstSample *sample = gst_app_sink_pull_sample (sink);
	GstBuffer *buffer = gst_sample_get_buffer (sample);
	GstMapInfo map;
	if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
		gst_sample_unref (sample);
		return GST_FLOW_ERROR;
	};
	/* One sample is 2 bytes (16 bit signed integer), see caps in
	 * setup_pipeline(). The buffer contains several samples, the number
	 * of elements/samples is map.size / sample size. */
	g_array_append_vals (wl->priv->hires, map.data, map.size / 2);
	gst_buffer_unmap (buffer, &map);
	gst_sample_unref (sample);

	/* If hires has more than one second of new data available, add it to
	 * lowres. The very last data will be added at the EOS signal. */
	gint hires_full_seconds = wl->priv->hires->len / 8000;
	gint lowres_full_seconds = wl->priv->lowres_index / 2 / wl->priv->lowres_pps;
	gint pps = wl->priv->lowres_pps;
	if (hires_full_seconds > lowres_full_seconds) {
		/* Make sure lowres is big enough */
		if (wl->priv->lowres_index + pps * 2 + 1 > wl->priv->lowres->len) {
			g_array_set_size (wl->priv->lowres, wl->priv->lowres_index + pps * 2 + 2);
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
					  "MESSAGE", "lowres->len resized in new_sample_cb: %d",
					  wl->priv->lowres_index + pps * 2 + 2);
		}
		convert_one_second (wl, lowres_full_seconds);
	}

	return GST_FLOW_OK;
}

static gboolean
setup_pipeline (PtWaveloader *wl)
{
	gboolean result = TRUE;
	GstElement *src, *dec, *conv, *resample, *sink;
	GstCaps *caps;

	/* create loader pipeline TODO: this is probably leaking on 2nd run */
	wl->priv->pipeline = gst_pipeline_new ("wave-loader");

	/* TODO gst_element_make_from_uri(): The URI must be gst_uri_is_valid(). */
	src 		   = gst_element_make_from_uri (GST_URI_SRC, wl->priv->uri, NULL, NULL);
	dec 		   = gst_element_factory_make ("decodebin", NULL);
	conv 		   = gst_element_factory_make ("audioconvert", NULL);
	resample           = gst_element_factory_make ("audioresample", NULL);
	wl->priv->fmt 	   = gst_element_factory_make ("capsfilter", NULL);
	sink 		   = gst_element_factory_make ("appsink", NULL);

	/* configure elements */
	caps = gst_caps_new_simple ("audio/x-raw",
				    "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
				    "layout", G_TYPE_STRING, "interleaved",
				    "channels", G_TYPE_INT, 1,
				    "rate", G_TYPE_INT, 8000, NULL);

	g_object_set (wl->priv->fmt, "caps", caps, NULL);
	gst_caps_unref (caps);

	g_object_set (sink, "emit-signals", TRUE, "sync", FALSE, NULL);

	/* add and link */
	gst_bin_add_many (GST_BIN (wl->priv->pipeline), src, dec, conv, resample, wl->priv->fmt, sink, NULL);
	result = gst_element_link (src, dec);
	if (!result) {
		GST_WARNING_OBJECT (wl->priv->pipeline,
			"Can’t link wave loader pipeline (src ! dec ! conv ! resample ! fmt ! sink).");
		return result;
	}

	result = gst_element_link_many (conv, resample, wl->priv->fmt, sink, NULL);
	if (!result) {
		GST_WARNING_OBJECT (wl->priv->pipeline,
			"Can’t link wave loader pipeline (conv ! resample ! fmt ! sink).");
		return result;
	}

	g_signal_connect (dec, "pad-added", G_CALLBACK (on_wave_loader_new_pad),
			(gpointer) conv);
	g_signal_connect (sink, "new-sample", G_CALLBACK (new_sample_cb), wl);

	return result;
}

static gboolean
check_progress (GTask *task)
{
	/* This timeout runs parallel to message bus (bus_handler()).
	   If it’s removed, the message bus has to be removed, too, and also
	   the other way round. */

	/* TODO think about moving all this to new_sample_cb() */

	PtWaveloader *wl = g_task_get_source_object (task);

	gint64  dur;
	gint64  pos;
	gdouble temp;
	gint new_size;

	/* Check if task was cancelled and reset pipeline */

	if (g_cancellable_is_cancelled (g_task_get_cancellable (task))) {
		gst_element_set_state (wl->priv->pipeline, GST_STATE_NULL);
		g_source_remove (wl->priv->bus_watch_id);
		wl->priv->bus_watch_id = 0;
		wl->priv->progress_timeout = 0;
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return G_SOURCE_REMOVE;
	}

	/* Query position and duration and emit progress signal */

	if (!gst_element_query_position (wl->priv->pipeline, GST_FORMAT_TIME, &pos))
		return G_SOURCE_CONTINUE;

	if (!gst_element_query_duration (wl->priv->pipeline, GST_FORMAT_TIME, &dur))
		return G_SOURCE_CONTINUE;

	if (dur > wl->priv->duration) {
		wl->priv->duration = dur;
		new_size = wl->priv->duration / 1000000000 * wl->priv->lowres_pps * 2;
		if (wl->priv->lowres->len != new_size) {
			g_array_set_size (wl->priv->lowres, new_size);
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
					  "MESSAGE", "Duration changed signal: %" GST_TIME_FORMAT" lowres resized to len %d",
					  GST_TIME_ARGS (wl->priv->duration), new_size);
			g_signal_emit_by_name (wl, "array-size-changed");
		}
	}

	temp = (gdouble) pos / dur;

	if (temp > wl->priv->progress && temp < 1) {
		wl->priv->progress = temp;
		g_signal_emit_by_name (wl, "progress", wl->priv->progress);
	}

	return G_SOURCE_CONTINUE;
}

static gboolean
bus_handler (GstBus     *bus,
             GstMessage *msg,
             gpointer    data)
{
	GTask *task = (GTask *) data;
	PtWaveloader *wl = g_task_get_source_object (task);

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ERROR: {
		gchar    *debug;
		GError   *error;

		remove_timeout (wl);
		gst_message_parse_error (msg, &error, &debug);

		/* Error is returned. Log the message here at level DEBUG,
		   as higher levels will abort tests. */

		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				  "MESSAGE", "Error from element %s: %s", GST_OBJECT_NAME (msg->src), error->message);
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				  "MESSAGE", "Debugging info: %s", (debug) ? debug : "none");
		g_free (debug);
		wl->priv->bus_watch_id = 0;
		g_task_return_error (task, error);
		g_object_unref (task);
		return FALSE;
		}

	case GST_MESSAGE_EOS: {

		/* convert remaining data from hires to lowres */
		g_array_set_size (wl->priv->lowres, calc_lowres_len (wl->priv->hires->len, wl->priv->lowres_pps));

		/* while hires has more full seconds than lowres ... */
		while (wl->priv->hires->len / 8000 > wl->priv->lowres_index / 2 / wl->priv->lowres_pps) {
			convert_one_second (wl, wl->priv->lowres_index / 2 / wl->priv->lowres_pps);
		}

		/* ... and now add the last data */
		convert_one_second (wl, (wl->priv->lowres_index / 2 / wl->priv->lowres_pps));

		/* query length and convert to samples */
		if (!gst_element_query_duration (wl->priv->pipeline, GST_FORMAT_TIME, &wl->priv->duration)) {
			GST_WARNING ("getting sample duration failed");
		}

		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				  "MESSAGE", "Sample decoded: hires->len=%d, lowres->len=%d, pps=%d, duration=%" GST_TIME_FORMAT,
		                  wl->priv->hires->len, wl->priv->lowres->len, wl->priv->lowres_pps, GST_TIME_ARGS (wl->priv->duration));

		remove_timeout (wl);
		wl->priv->bus_watch_id = 0;
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return FALSE;
		}
	case GST_MESSAGE_DURATION_CHANGED: {
		gst_element_query_duration (wl->priv->pipeline, GST_FORMAT_TIME, &wl->priv->duration);
		gint new_size;
		new_size = wl->priv->duration / 1000000000 * wl->priv->lowres_pps * 2;
		g_array_set_size (wl->priv->lowres, new_size);
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				  "MESSAGE", "Duration changed signal: %" GST_TIME_FORMAT" lowres resized to len %d",
				  GST_TIME_ARGS (wl->priv->duration), new_size);
		g_signal_emit_by_name (wl, "array-size-changed");
		break;
	}
	default:
		break;
	}

	return TRUE;
}

/**
 * pt_waveloader_load_finish:
 * @wl: a #PtWaveloader
 * @result: the #GAsyncResult passed to your #GAsyncReadyCallback
 * @error: (nullable): a pointer to a NULL #GError, or NULL
 *
 * Gives the result of the async load operation. A cancelled operation results
 * in an error, too.
 *
 * Return value: TRUE if successful, or FALSE with error set
 *
 * Since: 1.4
 */
gboolean
pt_waveloader_load_finish (PtWaveloader  *wl,
                           GAsyncResult  *result,
                           GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (result, wl), FALSE);

	wl->priv->load_pending = FALSE;
	g_signal_emit_by_name (wl, "progress", 1);
	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * pt_waveloader_load_async:
 * @wl: a #PtWaveloader
 * @pps: pixels per second
 * @cancellable: (nullable): a #GCancellable or NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the operation is complete
 * @user_data: (closure): user_data for callback
 *
 * Saves sample data to private memory. To keep the memory footprint low, the
 * raw data is downsampled to 8000 samples per second (16 bit per sample). This
 * requires approx. 1 MB of memory per minute.
 *
 * #PtWaveloader:uri must be set. If it is not valid, an error will be returned.
 * If there is another load operation going on, an error will be returned.
 *
 * While saving data #PtWaveloader::progress is emitted every 30 ms.
 *
 * In your callback call #pt_waveloader_load_finish to get the result of the
 * operation.
 *
 * You can get a pointer to the #GArray holding the data with #pt_waveloader_get_data
 * either before loading it or afterwards.
 *
 * Since: 2.0
 */
void
pt_waveloader_load_async (PtWaveloader        *wl,
                          gint                 pps,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
	g_return_if_fail (PT_IS_WAVELOADER (wl));
	g_return_if_fail (wl->priv->uri != NULL);

	GTask    *task;
	GstBus   *bus;

	task = g_task_new (wl, cancellable, callback, user_data);
	/* Lets have an initial size of 60 sec */
	g_array_set_size (wl->priv->lowres, pps * 60);
	wl->priv->lowres_pps = pps;
	wl->priv->lowres_index = 0;

	if (wl->priv->load_pending) {
		g_task_return_new_error (
				task,
				GST_CORE_ERROR,
				GST_CORE_ERROR_FAILED,
				_("Waveloader has outstanding operation."));
		g_object_unref (task);
		return;
	}

	wl->priv->load_pending = TRUE;
	wl->priv->progress = 0;
	g_array_set_size (wl->priv->hires, 0);

	/* setup pipeline TODO: do it just on init */
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

	/* Run pipeline and start timeout for progress and cancellation.
	 * Errors are reported on bus. */
	gst_element_set_state (wl->priv->pipeline, GST_STATE_PLAYING);
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
 *
 * Since: 1.4
 */
gint64
pt_waveloader_get_duration (PtWaveloader *wl)
{
	return wl->priv->duration;
}

/**
 * pt_waveloader_resize_finish:
 * @wl: a #PtWaveloader
 * @result: the #GAsyncResult passed to your #GAsyncReadyCallback
 * @error: (nullable): a pointer to a NULL #GError, or NULL
 *
 * Gives the result of the async resize operation. A cancelled operation results
 * in an error, too.
 *
 * Return value: TRUE if successful, or FALSE with error set
 *
 * Since: 2.0
 */
gboolean
pt_waveloader_resize_finish (PtWaveloader  *wl,
                             GAsyncResult  *result,
                             GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (result, wl), FALSE);

	wl->priv->data_pending = FALSE;
	g_signal_emit_by_name (wl, "progress", 1);
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
pt_waveloader_resize_real (GTask        *task,
			   gpointer      source_object,
			   gpointer      task_data,
		           GCancellable *cancellable)
{
	/* TODO share code with convert_one_second() */

	PtWaveloader *wl = source_object;
	gint pps = GPOINTER_TO_INT (task_data);
	gint k, m;
	gint16 d;
	gfloat vmin, vmax;
	gint chunk_size;
	gint mod;
	gint lowres_len;
	gboolean result = TRUE;

	lowres_len = calc_lowres_len (wl->priv->hires->len, pps);
	chunk_size = 8000 / pps;
	mod = 8000 % pps;
	if (wl->priv->lowres == NULL || wl->priv->lowres->len != lowres_len) {
		g_array_set_size (wl->priv->lowres, lowres_len);
		g_signal_emit_by_name (wl, "array-size-changed");
	}

	gint index_in = 0;
	gint index_out = 0;
	gdouble progress = 0;

	gint correct;
	while (index_in < wl->priv->hires->len && index_out < lowres_len) {
		progress = (gdouble) index_out / lowres_len;
		g_signal_emit_by_name (wl, "progress", progress);

		if (g_cancellable_is_cancelled (cancellable)) {
			result = FALSE;
			break;
		}
		/* Loop data worth 1 second */
		for (k = 0; k < pps; k++) {
			vmin = 0;
			vmax = 0;
			/* If there is a remainder for in_rate/out_rate, correct
			 * it by reading in an additional sample. */
			correct = 0;
			if (k < mod)
				correct = 1;
			for (m = 0; m < (chunk_size + correct); m++) {
				/* Get highest and lowest value */
				d = g_array_index (wl->priv->hires, gint16, index_in);
				if (d < vmin)
					vmin = d;
				if (d > vmax)
					vmax = d;
				index_in++;
				if (index_in == wl->priv->hires->len)
					break;
			}
			/* Always include 0, looks better at higher resolutions */
			if (vmin > 0 && vmax > 0)
				vmin = 0;
			else if (vmin < 0 && vmax < 0)
				vmax = 0;
			/* Save as a float in the range 0 to 1 */
			vmin = vmin / 32768.0;
			vmax = vmax / 32768.0;
			memcpy (wl->priv->lowres->data + index_out * sizeof (float), &vmin, sizeof (float));
			index_out++;
			memcpy (wl->priv->lowres->data + index_out * sizeof (float), &vmax, sizeof (float));
			index_out++;
			if (index_out > lowres_len - 2)
				break;
		}
	}

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "hires->len: %d", wl->priv->hires->len);
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Array size: %" G_GINT64_FORMAT " ",
	                  lowres_len);
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "samples: %" G_GINT64_FORMAT " ",
	                  lowres_len / 2);
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "pixels per sec: %d", pps);
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "index_in: %d", index_in);
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "index_out: %d", index_out);

	g_task_return_boolean (task, result); /* TODO unref task? */
}

/**
 * pt_waveloader_resize_async:
 * @wl: a #PtWaveloader
 * @pps: the requested pixel per second ratio
 * @cancellable: (nullable): a #GCancellable or NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the operation is complete
 * @user_data: (closure): user_data for callback
 *
 * Resizes wave form data at the requested resolution @pps. If the array's
 * size has to be changed, #PtWaveloader::array-size-changed is emitted.
 * Any data in the array will be overwritten.
 *
 * The resolution is given as pixel per seconds, e.g. 100 means one second
 * is represented by 100 samples, is 100 pixels wide. @pps must be >= 25 and <= 200.
 *
 * You should have loaded a file with #pt_waveloader_load_async before resizing
 * it. There are no concurrent operations allowed. An error is returned,
 * if the file was not loaded before, if it is still loading or if there is
 * another #pt_waveloader_get_data_async operation going on.
 *
 * Cancelling the operation returns an error and lets the array in an inconsistent
 * state. Its size will be according to the requested new resolution, but the data
 * will be partly old, partly new.
 *
 * In your callback call #pt_waveloader_get_data_finish to get the result of the
 * operation.
 *
 * Since: 2.0
 */
void
pt_waveloader_resize_async (PtWaveloader        *wl,
			    gint                 pps,
			    GCancellable        *cancellable,
			    GAsyncReadyCallback  callback,
			    gpointer             user_data)
{
	g_return_if_fail (PT_IS_WAVELOADER (wl));
	g_return_if_fail ((pps >= 25) && (pps <= 200));

	GTask    *task;

	task = g_task_new (wl, cancellable, callback, user_data);
	if (wl->priv->hires->len == 0) {
		g_task_return_new_error (
				task,
				GST_CORE_ERROR,
				GST_CORE_ERROR_FAILED,
				_("No Array!"));
		g_object_unref (task);
		return;
	}
	if (wl->priv->load_pending || wl->priv->data_pending) {
		g_task_return_new_error (
				task,
				GST_CORE_ERROR,
				GST_CORE_ERROR_FAILED,
				_("Waveloader has outstanding operation."));
		g_object_unref (task);
		return;
	}

	wl->priv->data_pending = TRUE;
	g_task_set_task_data (task, GINT_TO_POINTER (pps), NULL);
	g_task_run_in_thread (task, pt_waveloader_resize_real);
	g_object_unref (task);
}

typedef struct
{
	GAsyncResult *res;
	GMainLoop    *loop;
} SyncData;

static void
quit_loop_cb (PtWaveloader *wl,
              GAsyncResult *res,
              gpointer      user_data)
{
	SyncData *data = user_data;
	data->res = g_object_ref (res);
	g_main_loop_quit (data->loop);
}

/**
 * pt_waveloader_resize:
 * @wl: a #PtWaveloader
 * @pps: the requested pixel per second ratio
 * @error: (nullable): return location for an error, or NULL
 *
 * Sync version of #pt_waveloader_resize_async.
 *
 * Return value: TRUE if successful, or FALSE with error set
 *
 * Since: 2.0
 */
gboolean
pt_waveloader_resize (PtWaveloader *wl,
		      gint          pps,
		      GError      **error)
{
	g_return_val_if_fail (PT_IS_WAVELOADER (wl), FALSE);
	g_return_val_if_fail ((pps >= 25) && (pps <= 200), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	gboolean      result;
	SyncData      data;
	GMainContext *context;

	/* Note: If done in the default thread, all sorts of bad things happen,
	 * mostly segfaults in painting, freezes etc. I have to admit that
	 * I don't fully understand this. – Signals are not emitted now. */
	context = g_main_context_new ();
	g_main_context_push_thread_default (context);

	data.loop = g_main_loop_new (context, FALSE);
	data.res = NULL;

	pt_waveloader_resize_async (wl, pps, NULL, (GAsyncReadyCallback) quit_loop_cb, &data);
	g_main_loop_run (data.loop);

	result = pt_waveloader_resize_finish (wl, data.res, error);

	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);
	g_main_loop_unref (data.loop);
	g_object_unref (data.res);

	return result;
}

/**
 * pt_waveloader_get_data:
 * @wl: a #PtWaveloader
 *
 * This returns the pointer to a GArray that is holding all the wave form data.
 * It consists of a minimum value in the range of -1 to 0 and a maximum value
 * in the range of 0 to 1, repeating this until the end. Each min/max pair can
 * be drawn by the application as one vertical line in the wave form.
 *
 * Don't modify the data.
 *
 * Return value: (element-type float) (transfer none): a #GArray with wave data
 *
 * Since: 2.0
 */
GArray *
pt_waveloader_get_data (PtWaveloader *wl)
{
	return wl->priv->lowres;
}
/* --------------------- Init and GObject management ------------------------ */

static void
pt_waveloader_init (PtWaveloader *wl)
{
	wl->priv = pt_waveloader_get_instance_private (wl);

	GError *gst_error = NULL;
	gst_init_check (NULL, NULL, &gst_error);
	if (gst_error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
			          "MESSAGE", "PtWaveloader failed to init GStreamer: %s", gst_error->message);
		g_clear_error (&gst_error);
	}

	wl->priv->pipeline = NULL;
	wl->priv->uri = NULL;
	wl->priv->hires = g_array_new (FALSE,	/* zero terminated */
	                               TRUE,	/* clear to zero   */
	                               sizeof(gint16));
	wl->priv->lowres = g_array_new (FALSE, TRUE, sizeof(float));
	wl->priv->bus_watch_id = 0;
	wl->priv->progress_timeout = 0;
	wl->priv->load_pending = FALSE;
	wl->priv->data_pending = FALSE;
}

static void
pt_waveloader_dispose (GObject *object)
{
	PtWaveloader *wl;
	wl = PT_WAVELOADER (object);

	g_free (wl->priv->uri);

	g_array_unref (wl->priv->hires);

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
	signals[PROGRESS] =
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
	* PtWaveloader::array-size-changed:
	* @wl: the waveloader emitting the signal
	*
	* The size of the array with waveform data has changed.
	*/
	signals[ARRAY_SIZE_CHANGED] =
	g_signal_new ("array-size-changed",
		      PT_TYPE_WAVELOADER,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	/**
	* PtWaveloader:uri:
	*
	* URI of the audio file.
	*/
	obj_properties[PROP_URI] =
	g_param_spec_string (
			"uri",
			"URI to load from",
			"URI to load from",
			"",
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

/**
 * pt_waveloader_new:
 * @uri: (nullable): URI of the audio file to load
 *
 * Returns a new #PtWaveloader. The @uri is not checked on construction, but
 * calling pt_waveloader_load_async() will fail with an error message.
 *
 * After use g_object_unref() it.
 *
 * Return value: (transfer full): a new #PtWaveloader
 *
 * Since: 1.4
 */
PtWaveloader *
pt_waveloader_new (gchar *uri)
{
	_pt_i18n_init ();
	return g_object_new (PT_TYPE_WAVELOADER,
			     "uri", uri,
			     NULL);
}
