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
#include <glib/gi18n.h>
#include <string.h>
#include <atspi/atspi.h>
#include "pt-app.h"
#include "pt-asr-output.h"


struct _PtAsrOutputPrivate
{
	gboolean            atspi;
	GtkWidget          *dialog;
	GtkWidget          *textpad;
	GtkTextBuffer      *textbuffer;
	AtspiEventListener *focus_listener;
	AtspiEventListener *caret_listener;
	AtspiAccessible    *app;
	AtspiAccessible    *editable;
	gchar              *app_name;
	gint                offset;
	gint                hyp_len;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtAsrOutput, pt_asr_output, G_TYPE_OBJECT)


static gboolean
register_listener (AtspiEventListener *listener,
                   const gchar        *event_type)
{
	GError   *error = NULL;
	gboolean  success;

	success = atspi_event_listener_register (listener, event_type, &error);
	if (error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				  "MESSAGE", "Registering AtspiEventListener failed: %s", error->message);
		g_clear_error (&error);
	}

	return success;
}

static gboolean
deregister_listener (AtspiEventListener *listener,
                     const gchar        *event_type)
{
	GError   *error = NULL;
	gboolean  success;

	success = atspi_event_listener_deregister (listener, event_type, &error);
	if (error) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				  "MESSAGE", "Deregistering AtspiEventListener failed: %s", error->message);
		g_clear_error (&error);
	}

	return success;
}

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
on_caret_cb (AtspiEvent *event,
             void       *user_data)
{
	PtAsrOutput *self = PT_ASR_OUTPUT (user_data);

	if (!event->source)
		return;

	if (!is_editable (event->source))
		return;

	if (event->source != self->priv->editable) {

		/* The caret is at a different paragraph. This can be done
		   only by the user. Update our output editable. */

		g_object_unref (self->priv->editable);
		self->priv->editable = g_object_ref (event->source);
	}
}

static void
on_focus_cb (AtspiEvent *event,
             void       *user_data)
{
	PtAsrOutput *self = PT_ASR_OUTPUT (user_data);

	if (!event->source)
		return;

	/* We only care about focus/selection gain
	   there’s no detail1 on focus loss in AT-SPI specs
	   taken from accessibilitywatcher.cpp from compiz-plugins-main */
	if (!event->detail1)
		return;

	if (!is_editable (event->source))
		return;
	
	self->priv->app = atspi_accessible_get_application (event->source, NULL);
	if (self->priv->app == NULL)
		return;

	self->priv->app_name = atspi_accessible_get_name (self->priv->app, NULL);
	if (g_strcmp0 (self->priv->app_name, "gnome-shell") == 0) {
		/* That’s the search field of gnome shell when trying to find
		   and launch a program */
		g_free (self->priv->app_name);
		return;
	}

	self->priv->editable = g_object_ref (event->source);

	deregister_listener (self->priv->focus_listener, "object:state-changed:focused");

	/* LibreOffice needs extra care: each paragraph is an AtspiEditable of
	   its own. If the user moves the cursor/caret to another paragraph,
	   we still reference the old one and there is no output at all. */

	if (g_strcmp0 (self->priv->app_name, "soffice") == 0) {
		g_free (self->priv->app_name);
		self->priv->app_name = g_strdup ("LibreOffice");
		register_listener (self->priv->caret_listener,
		                   "object:text-caret-moved");
		/* listener stays active until object destruction */
	}

	gtk_widget_destroy (self->priv->dialog);
	g_signal_emit_by_name (self, "app-found");
}

static void
pt_asr_output_reset (PtAsrOutput *self)
{
	g_clear_object (&self->priv->app);
	g_clear_object (&self->priv->editable);
	g_free (self->priv->app_name);
	self->priv->app_name = NULL;
}

static void
pt_asr_output_cancel_search (PtAsrOutput *self)
{
	deregister_listener (self->priv->focus_listener, "object:state-changed:focused");
	pt_asr_output_reset (self);
}

static void
dialog_response_cb (GtkDialog   *dialog,
                    gint         response_id,
                    PtAsrOutput *self)
{
	/* every response means cancel */

	pt_asr_output_cancel_search (self);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_dialog (PtAsrOutput *self,
             GtkWindow   *parent)
{
	gchar     *message;
	gchar     *secondary_message;

	/* Translators: This feature is disabled by default, its translation has low priority. */
	message = _("Select output for automatic speech recognition");
	/* Translators: This feature is disabled by default, its translation has low priority. */
	secondary_message = _("Please open and switch focus to a word processor.");

        self->priv->dialog = gtk_message_dialog_new (
			parent,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_CANCEL,
			"%s", message);

	gtk_message_dialog_format_secondary_text (
			GTK_MESSAGE_DIALOG (self->priv->dialog),
	                "%s", secondary_message);

	g_signal_connect (self->priv->dialog,
                          "response",
                          G_CALLBACK (dialog_response_cb),
                          self);

	gtk_widget_show (self->priv->dialog);
}

void
create_textpad (PtAsrOutput *self,
                GtkWindow   *parent)
{
	GtkWidget *textview;
	GtkWidget *scrolled_window;

	self->priv->textpad = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (self->priv->textpad), "Textpad");

	textview = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
	self->priv->textbuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);

	gtk_container_add (GTK_CONTAINER (scrolled_window), textview);
	gtk_container_add (GTK_CONTAINER (self->priv->textpad), scrolled_window);

	gtk_widget_show_all (scrolled_window);
	gtk_window_set_default_size (GTK_WINDOW (self->priv->textpad), 400, 200);
	gtk_window_present (GTK_WINDOW (self->priv->textpad));
}

void
pt_asr_output_search_app (PtAsrOutput *self,
                          GtkWindow   *parent)
{
	pt_asr_output_reset (self);
	if (self->priv->atspi) {
		if (!register_listener (self->priv->focus_listener,
			                "object:state-changed:focused")) {
			self->priv->atspi = FALSE;
		}
	}

	if (!self->priv->atspi) {
		create_textpad (self, parent);
		self->priv->app_name = g_strdup ("Textpad");
		g_signal_emit_by_name (self, "app-found");
		return;
	}

	show_dialog (self, parent);
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

	if (self->priv->atspi) {
		atspi_editable_text_delete_text (
				ATSPI_EDITABLE_TEXT (self->priv->editable),
				self->priv->offset, self->priv->offset + self->priv->hyp_len, NULL);
	} else {
		GtkTextIter iter1, iter2;
		gtk_text_buffer_get_iter_at_offset (self->priv->textbuffer, &iter1, self->priv->offset);
		gtk_text_buffer_get_iter_at_offset (self->priv->textbuffer, &iter2, self->priv->offset + self->priv->hyp_len);
		gtk_text_buffer_delete (self->priv->textbuffer, &iter1, &iter2);
	}
}

static void
get_offset (PtAsrOutput *self)
{
	if (self->priv->atspi) {
		self->priv->offset = atspi_text_get_caret_offset (ATSPI_TEXT (self->priv->editable), NULL);
	} else {
		self->priv->offset = gtk_text_buffer_get_char_count (self->priv->textbuffer);
	}
}

void
pt_asr_output_hypothesis (PtAsrOutput *self,
                          gchar       *string)
{
	/* TODO A long hypothesis, quickly updated, can flicker. Try to reduce
	        flickering by comparing the old string and new string from left
	        (what about RTL?) until there is a difference and output only the rest.
	        Store the old string, compare, delete with new offsets */

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Hypothesis: %s", string);

	delete_hypothesis (self);	

	GtkTextIter iter;
	get_offset (self);
	self->priv->hyp_len = g_utf8_strlen (string, -1);

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Offset: %d – Length: %d", self->priv->offset, self->priv->hyp_len);

	/* inserting moves caret, important: use strlen, not UTF-8 character count */
	if (self->priv->atspi) {
		atspi_editable_text_insert_text (
				ATSPI_EDITABLE_TEXT (self->priv->editable),
				self->priv->offset, string, strlen (string), NULL);
	} else {
		gtk_text_buffer_get_iter_at_offset (self->priv->textbuffer, &iter, self->priv->offset);
		gtk_text_buffer_insert (self->priv->textbuffer, &iter, string, strlen (string));
	}
}

void
pt_asr_output_final (PtAsrOutput *self,
                     gchar       *string)
{
	gchar *string_with_space;
	GtkTextIter iter;

	delete_hypothesis (self);

	get_offset (self);
	self->priv->hyp_len = 0;

	string_with_space = g_strdup_printf ("%s ", string);

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Final: %s", string);

	/* inserting moves caret, important: use strlen, not UTF-8 character count */
	if (self->priv->atspi) {
		atspi_editable_text_insert_text (
				ATSPI_EDITABLE_TEXT (self->priv->editable),
				self->priv->offset, string_with_space,
				strlen (string_with_space), NULL);
	} else {
		gtk_text_buffer_get_iter_at_offset (self->priv->textbuffer, &iter, self->priv->offset);
		gtk_text_buffer_insert (self->priv->textbuffer, &iter, string_with_space, strlen (string_with_space));
	}
	g_free (string_with_space);
}

static void
pt_asr_output_init (PtAsrOutput *self)
{
	self->priv = pt_asr_output_get_instance_private (self);

	GApplication *app;
	gint          status;

	app = g_application_get_default ();
	self->priv->atspi = pt_app_get_atspi (PT_APP (app));

	if (self->priv->atspi)
		status = atspi_init ();
	else
		status = 3;

	if (status == 0 || status == 1) {
		self->priv->focus_listener = atspi_event_listener_new (
				(AtspiEventListenerCB) on_focus_cb, self, NULL);
		self->priv->caret_listener = atspi_event_listener_new (
				(AtspiEventListenerCB) on_caret_cb, self, NULL);
		if (status == 0) {
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					  "MESSAGE", "ATSPI initted");
		} else {
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					  "MESSAGE", "ATSPI already initted");
		}
	} else {
		self->priv->atspi = FALSE;
		self->priv->focus_listener = NULL;
		self->priv->caret_listener = NULL;
		if (status == 2) {
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					  "MESSAGE", "ATSPI failed");
		} else {
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
					  "MESSAGE", "ATSPI fallback requested");
		}
	}
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
	g_clear_object (&self->priv->focus_listener);
	g_clear_object (&self->priv->caret_listener);

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

	g_signal_new ("search-cancelled",
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
