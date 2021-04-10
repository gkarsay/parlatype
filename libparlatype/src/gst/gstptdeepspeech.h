/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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


#ifndef __GST_PTDEEPSPEECH_H__
#define __GST_PTDEEPSPEECH_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "vad_private.h"
#include "deepspeech.h"

G_BEGIN_DECLS

#define GST_TYPE_PTDEEPSPEECH             (gst_ptdeepspeech_get_type())
#define GST_PTDEEPSPEECH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PTDEEPSPEECH,GstPtDeepspeech))
#define GST_IS_PTDEEPSPEECH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PTDEEPSPEECH))
#define GST_PTDEEPSPEECH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PTDEEPSPEECH,GstPtDeepspeechClass))
#define GST_IS_PTDEEPSPEECH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PTDEEPSPEECH))

typedef struct _GstPtDeepspeech      GstPtDeepspeech;
typedef struct _GstPtDeepspeechClass GstPtDeepspeechClass;

struct _GstPtDeepspeech
{
	/* GStreamer internals */
	GstBaseTransform  element;
	GstPad           *sinkpad;
	GstPad           *srcpad;

	/* Deepspeech */
	gchar            *speech_model_path;
	gchar            *scorer_path;
	ModelState       *model_state;
	StreamingState   *streaming_state;
	gint              beam_width;

	/* Voice Activity Detection (VAD) */
	VADFilter        *vad;
	gint              quiet_bufs;
	gdouble           silence_threshold;
	gint              silence_length;

	gboolean silent;
	guint16 minimum_silence_buffers;
	guint64 minimum_silence_time;

	guint64 ts_offset;
	gboolean silence_detected;
	guint64 consecutive_silence_buffers;
	guint64 consecutive_silence_time;
	GstClockTime last_result_time; /**< Timestamp of last partial result. */
};

struct _GstPtDeepspeechClass
{
	GstElementClass parent_class;
};

GType		gst_ptdeepspeech_get_type	(void);

gboolean	gst_ptdeepspeech_register	(void);

G_END_DECLS

#endif /* __GST_PTDEEPSPEECH_H__ */
