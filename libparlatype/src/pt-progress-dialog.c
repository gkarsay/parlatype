/* Copyright (C) 2016, 2017 Gabor Karsay <gabor.karsay@gmx.at>
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
#include <math.h>	/* ceil */
#include <gtk/gtk.h>
#define GETTEXT_PACKAGE "libparlatype"
#include <glib/gi18n-lib.h>
#include "pt-progress-dialog.h"


struct _PtProgressDialogPrivate
{
	GtkWidget *progressbar;
	GTimer    *timer;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtProgressDialog, pt_progress_dialog, GTK_TYPE_MESSAGE_DIALOG)

/**
 * SECTION: pt-progress-dialog
 * @short_description: Progress dialog while loading wave data.
 * @stability: Unstable
 * @include: parlatype/pt-progress-dialog.h
 *
 * PtProgressDialog is a ready to use dialog, intended to show progress while
 * loading wave data. Note, that it is supposed to be shown via
 * gtk_widget_show_all() and loading wave data should be done asynchronously.
 * Using the synchronous version the dialog will not show up.
 *
 * Assuming you have set up a PtPlayer *player, typical usage would be:
 * |[<!-- language="C" -->
 * static void
 * progress_response_cb (GtkWidget *dialog,
 *                       gint       response,
 *                       gpointer  *user_data)
 * {
 *     PtPlayer *player = (PtPlayer *) user_data;
 *
 *     if (response == GTK_RESPONSE_CANCEL)
 *         pt_player_cancel (player);
 * }
 *
 * ...
 *
 * static void
 * open_cb (PtPlayer     *player,
 *          GAsyncResult *res,
 *          gpointer     *user_data)
 * {
 *     PtProgressDialog *dlg = (PtProgressDialog *) user_data;
 *     gtk_widget_destroy (GTK_WIDGET (dlg));
 *     ...
 * }
 *
 * ...
 *
 * progress_dlg = GTK_WIDGET (pt_progress_dialog_new (GTK_WINDOW (parent)));
 *
 * g_signal_connect (progress_dlg,
 *                   "response",
 *                   G_CALLBACK (progress_response_cb),
 *                   player);
 *
 * g_signal_connect_swapped (player,
 *                           "load-progress",
 *                           G_CALLBACK (pt_progress_dialog_set_progress),
 *                           PT_PROGRESS_DIALOG (progress_dlg));
 *
 * gtk_widget_show_all (win->priv->progress_dlg);
 * pt_player_open_uri_async (player,
 *                           uri,
 *                           (GAsyncReadyCallback) open_cb,
 *                           progress_dlg);
 *
 * ]|
 */


static gchar*
format_time_string (gint seconds)
{
	gchar *result;

	if (seconds <= 60) {
		result = g_strdup_printf (
				g_dngettext (GETTEXT_PACKAGE,
				             "Time remaining: 1 second",
				             "Time remaining: %d seconds",
				             seconds),
				seconds);
		return result;
	}

	if (seconds > 60) {
		result = g_strdup_printf (
				/* Translators: this is a time with minutes
				   and seconds, e.g. Time remaining: 3:20 */
				_("Time remaining: %d:%02d"),
				seconds / 60, seconds % 60);
		return result;
	}

	g_assert_not_reached ();
}


/**
 * pt_progress_dialog_set_progress:
 * @dlg: the dialog
 * @progress: the new value for the progress bar, ranging from 0.0 to 1.0
 *
 * Sets the progress bar to the new value.
 */
void
pt_progress_dialog_set_progress (PtProgressDialog *dlg,
                                 gdouble           progress)
{
	g_return_if_fail (PT_IS_PROGRESS_DIALOG (dlg));
	g_return_if_fail (progress >= 0 && progress <= 1);

	gdouble  elapsed;	/* seconds */
	gint     remaining;	/* seconds */
	gchar   *message;

	elapsed = g_timer_elapsed (dlg->priv->timer, NULL);
	remaining = ceil ((elapsed / progress) * (100 - (progress * 100)) / 100);
	message = format_time_string (remaining);

	gtk_progress_bar_set_fraction (
			GTK_PROGRESS_BAR (dlg->priv->progressbar),
			progress);
	gtk_progress_bar_set_text (
			GTK_PROGRESS_BAR (dlg->priv->progressbar),
			message);

	g_free (message);
}

static void
pt_progress_dialog_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (pt_progress_dialog_parent_class)->realize (widget);

	/* Give the dialog a fixed width, so that the size doesn’t change
	   when the count jumps from 10 to 9. Probably it isn’t exact
	   because depending on plural forms of the current locale the widest
	   string might be another one. */

	PtProgressDialog *dlg = PT_PROGRESS_DIALOG (widget);
	gchar *test_message;
	GtkRequisition natural_size;

	test_message = format_time_string (60);
	gtk_progress_bar_set_text (
			GTK_PROGRESS_BAR (dlg->priv->progressbar),
			test_message);
	gtk_widget_get_preferred_size (
			GTK_WIDGET (dlg),
			NULL,
			&natural_size);
	gtk_widget_set_size_request (
			GTK_WIDGET (dlg),
			natural_size.width + 10, /* +10px safety padding */
			-1);
	g_free (test_message);
}

static void
pt_progress_dialog_dispose (GObject *object)
{
	PtProgressDialog *dlg = PT_PROGRESS_DIALOG (object);
	if (dlg->priv->timer) {
		g_timer_destroy (dlg->priv->timer);
		dlg->priv->timer = NULL;
	}
	G_OBJECT_CLASS (pt_progress_dialog_parent_class)->dispose (object);
}

static void
pt_progress_dialog_init (PtProgressDialog *dlg)
{
	dlg->priv = pt_progress_dialog_get_instance_private (dlg);

	GtkWidget *content;

	dlg->priv->timer = g_timer_new ();
	dlg->priv->progressbar = gtk_progress_bar_new ();
	gtk_progress_bar_set_show_text (
			GTK_PROGRESS_BAR (dlg->priv->progressbar),
			TRUE);
	content = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dlg));
	gtk_container_add (GTK_CONTAINER (content), dlg->priv->progressbar);
}

static void
pt_progress_dialog_class_init (PtProgressDialogClass *klass)
{
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS (klass);

	gobject_class->dispose = pt_progress_dialog_dispose;
	widget_class->realize  = pt_progress_dialog_realize;
}

/**
 * pt_progress_dialog_new:
 * @win: (nullable): parent window, NULL is allowed, but discouraged
 *
 * A #GtkMessageDialog with a label "Loading file...", a progress bar and a
 * cancel button.
 *
 * After use gtk_widget_destroy() it.
 *
 * Return value: (transfer none): a new #PtProgressDialog
 */

/* Note on return value: There is a transfer but memory management is done
   by the user with gtk_widget_destroy() and not automatically by the binding. */

PtProgressDialog *
pt_progress_dialog_new (GtkWindow *win)
{
	return g_object_new (
			PT_TYPE_PROGRESS_DIALOG,
			"buttons", GTK_BUTTONS_CANCEL,
			"message_type", GTK_MESSAGE_OTHER,
			"text", _("Loading file…"),
			"modal", TRUE,
			"transient-for", win,
			"image", NULL,	/* needed for Ubuntu only, property exists
					   on vanilla GTK+ including 3.22, but is
					   not used, Ubuntu’s GTK+ patches use it */
			NULL);
}
