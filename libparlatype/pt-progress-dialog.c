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


#include <gtk/gtk.h>
#include <glib/gi18n.h>
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
			NULL);
}
