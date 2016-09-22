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
#include <glib/gi18n.h>
#include "pt-progress-dialog.h"


struct _PtProgressDialogPrivate
{
	GtkWidget *progressbar;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtProgressDialog, pt_progress_dialog, GTK_TYPE_MESSAGE_DIALOG)

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
