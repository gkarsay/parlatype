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


#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <pt-config.h>
#include "pt-asr-dialog.h"

#define MAX_URI_LENGTH 30

struct _PtAsrDialogPrivate
{
	GtkWidget *link_button;
	GtkWidget *folder_button;
	PtConfig  *config;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtAsrDialog, pt_asr_dialog, GTK_TYPE_WINDOW)


void
pt_asr_dialog_set_config (PtAsrDialog *dlg,
                          PtConfig    *config)
{
	gchar *link_uri = NULL;
	gchar *base_folder = NULL;
	GtkWidget *label;

	dlg->priv->config = config;

	gtk_window_set_title (GTK_WINDOW (dlg),
	                      pt_config_get_name (config));

	link_uri = pt_config_get_url (config);
	gtk_link_button_set_uri (GTK_LINK_BUTTON (dlg->priv->link_button),
	                         link_uri);
	gtk_button_set_label (GTK_BUTTON (dlg->priv->link_button), link_uri);

	/* Ellipsize label: This works only after gtk_button_set_label() */
	label = gtk_bin_get_child (GTK_BIN (dlg->priv->link_button));
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

	base_folder = pt_config_get_base_folder (config);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg->priv->folder_button),
	                                     base_folder);

	g_free (base_folder);
	g_free (link_uri);
}

static void
file_set_cb (GtkFileChooserButton *widget,
             gpointer              user_data)
{
	PtAsrDialog *dlg = PT_ASR_DIALOG (user_data);
	gchar *path;

	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	pt_config_set_base_folder (dlg->priv->config, path);

	g_free (path);
}

static void
pt_asr_dialog_init (PtAsrDialog *dlg)
{
	dlg->priv = pt_asr_dialog_get_instance_private (dlg);
	gtk_widget_init_template (GTK_WIDGET (dlg));
}

static void
pt_asr_dialog_class_init (PtAsrDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/asr-dialog.ui");
	gtk_widget_class_bind_template_callback(widget_class, file_set_cb);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, link_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, folder_button);

	g_signal_new ("removeme",
	              PT_TYPE_ASR_DIALOG,
	              G_SIGNAL_RUN_FIRST,
	              0,
	              NULL,
	              NULL,
	              g_cclosure_marshal_VOID__VOID,
	              G_TYPE_NONE,
	              0);
}

PtAsrDialog *
pt_asr_dialog_new (GtkWindow *win)
{
	return g_object_new (PT_TYPE_ASR_DIALOG, "transient-for", win, NULL);
}
