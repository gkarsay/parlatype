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


#include "config.h"
#include <gtk/gtk.h>
#define GETTEXT_PACKAGE PACKAGE
#include <glib/gi18n-lib.h>
#include "pt-progress-dialog.h"


struct _PtProgressDialogPrivate
{
	GtkWidget *progressbar;
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


/**
 * pt_progress_dialog_set_progress:
 * @dlg: the dialog
 * @progress: the new value for the progress bar, ranging from 0.0 to 1.0
 *
 * Sets the progress bar to the new value.
 */
void
pt_progress_dialog_set_progress (PtProgressDialog *dlg,
				 gdouble	   progress)
{
	g_return_if_fail (PT_IS_PROGRESS_DIALOG (dlg));
	g_return_if_fail (progress >= 0 && progress <= 1);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dlg->priv->progressbar), progress);
}

static void
pt_progress_dialog_init (PtProgressDialog *dlg)
{
	dlg->priv = pt_progress_dialog_get_instance_private (dlg);

	GtkWidget *content;
	dlg->priv->progressbar = gtk_progress_bar_new ();
	content = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dlg));
	gtk_container_add (GTK_CONTAINER (content), dlg->priv->progressbar);
}

static void
pt_progress_dialog_class_init (PtProgressDialogClass *klass)
{
	/* We don't do anything here */
}

/**
 * pt_progress_dialog_new:
 * @win: (allow-none): parent window, NULL is allowed, but discouraged
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
			"text", _("Loading file..."),
			"modal", TRUE,
			"transient-for", win,
			"image", NULL,	/* needed for Ubuntu only, property exists
					   on vanilla GTK+ including 3.22, but is
					   not used, Ubuntu's GTK+ patches use it */
			NULL);
}
