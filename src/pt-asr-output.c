/* Copyright (C) Gabor Karsay 2019 <gabor.karsay@gmx.at>
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
#include <atspi/atspi.h>
#include "pt-asr-output.h"


struct _PtAsrOutputPrivate
{
	AtspiEventListener *listener;
	AtspiAccessible    *app;
	AtspiAccessible    *editable;
	gchar              *app_name;
	gint                offset;
	gint                hyp_len;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtAsrOutput, pt_asr_output, G_TYPE_OBJECT)


static gboolean
is_editable (AtspiAccessible *accessible)
{
	AtspiStateSet *state_set;
	GArray        *states;
	gint           i;
	AtspiStateType state;
	gboolean       result = FALSE;

	state_set = atspi_accessible_get_state_set (accessible);
	states = atspi_state_set_get_states (state_set);
	for (i = 0; i < states->len; i++) {
		state = g_array_index (states, gint, i);
		if (state == ATSPI_STATE_EDITABLE) {
			result = TRUE;
			break;
		}
	}

	g_array_free (states, TRUE);
	g_object_unref (state_set);

	return result;
}

static void
on_event_cb (AtspiEvent *event,
             void       *user_data)
{
	PtAsrOutput *self = PT_ASR_OUTPUT (user_data);

	if (!event->source)
		return;

	/* We only care about focus/selection gain
	   there's no detail1 on focus loss in AT-SPI specs
	   taken from accessibilitywatcher.cpp from compiz-plugins-main */
	if (!event->detail1)
		return;

	if (!is_editable (event->source))
		return;
	
	self->priv->app = atspi_accessible_get_application (event->source, NULL);
	if (self->priv->app == NULL)
		return;

	self->priv->app_name = atspi_accessible_get_name (self->priv->app, NULL);
	self->priv->editable = g_object_ref (event->source);

	atspi_event_listener_deregister (self->priv->listener, "object:state-changed:focused", NULL);
	g_signal_emit_by_name (self, "app-found");
}

void
pt_asr_output_reset (PtAsrOutput *self)
{
	g_clear_object (&self->priv->app);
	g_clear_object (&self->priv->editable);
	g_free (self->priv->app_name);
	self->priv->app_name = NULL;
}

void
pt_asr_output_cancel_search (PtAsrOutput *self)
{
	atspi_event_listener_deregister (self->priv->listener, "object:state-changed:focused", NULL);
	pt_asr_output_reset (self);
}

void
pt_asr_output_search_app (PtAsrOutput *self)
{
	pt_asr_output_reset (self);
	atspi_event_listener_register (self->priv->listener, "object:state-changed:focused", NULL);
}

gchar*
pt_asr_output_get_app_name (PtAsrOutput *self)
{
	return self->priv->app_name;
}

static void
delete_hypothesis (PtAsrOutput *self)
{
	if (self->priv->hyp_len == 0)
		return;

	atspi_editable_text_delete_text (
			ATSPI_EDITABLE_TEXT (self->priv->editable),
			self->priv->offset, self->priv->offset + self->priv->hyp_len, NULL);
}

void
pt_asr_output_hypothesis (PtAsrOutput *self,
                          gchar       *string)
{
	delete_hypothesis (self);	
	self->priv->offset = atspi_text_get_caret_offset (ATSPI_TEXT (self->priv->editable), NULL);
	self->priv->hyp_len = g_utf8_strlen (string, -1);
	/* inserting moves caret */
	atspi_editable_text_insert_text (
			ATSPI_EDITABLE_TEXT (self->priv->editable),
			self->priv->offset, string, self->priv->hyp_len, NULL);
}

void
pt_asr_output_final (PtAsrOutput *self,
                     gchar       *string)
{
	gint len;

	delete_hypothesis (self);
	self->priv->offset = atspi_text_get_caret_offset (ATSPI_TEXT (self->priv->editable), NULL);
	self->priv->hyp_len = 0;
	len = g_utf8_strlen (string, -1);
	/* inserting moves caret */
	atspi_editable_text_insert_text (
			ATSPI_EDITABLE_TEXT (self->priv->editable),
			self->priv->offset, string, len, NULL);
}

static void
pt_asr_output_init (PtAsrOutput *self)
{
	self->priv = pt_asr_output_get_instance_private (self);

	atspi_init ();
	self->priv->listener = atspi_event_listener_new (
			(AtspiEventListenerCB) on_event_cb, self, NULL);
	self->priv->app = NULL;
	self->priv->editable = NULL;
	self->priv->app_name = NULL;
	self->priv->offset = 0;
	self->priv->hyp_len = 0;
}

static void
pt_asr_output_dispose (GObject *object)
{
	PtAsrOutput *self = PT_ASR_OUTPUT (object);

	g_clear_object (&self->priv->app);
	g_clear_object (&self->priv->editable);
	g_clear_object (&self->priv->listener);

	G_OBJECT_CLASS (pt_asr_output_parent_class)->dispose (object);
}

static void
pt_asr_output_finalize (GObject *object)
{
	PtAsrOutput *self = PT_ASR_OUTPUT (object);

	g_free (self->priv->app_name);

	G_OBJECT_CLASS (pt_asr_output_parent_class)->finalize (object);
}

static void
pt_asr_output_class_init (PtAsrOutputClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = pt_asr_output_dispose;
	object_class->finalize = pt_asr_output_finalize;

	g_signal_new ("app-found",
		      PT_TYPE_ASR_OUTPUT,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);
}

PtAsrOutput *
pt_asr_output_new (void)
{
	return g_object_new (PT_TYPE_ASR_OUTPUT, NULL);
}
