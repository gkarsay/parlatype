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
#include <parlatype.h>
#include "pt-asr-dialog.h"

#define MAX_URI_LENGTH 30

struct _PtAsrDialogPrivate
{
	PtConfig  *config;

	GtkWidget *name_entry;
	GtkWidget *lang_value;
	GtkWidget *engine_label;
	GtkWidget *engine_value;
	GtkWidget *plugin_label;
	GtkWidget *plugin_value;
	GtkWidget *publisher_label;
	GtkWidget *publisher_value;
	GtkWidget *license_label;
	GtkWidget *license_value;

	GtkWidget *howto_label;
	GtkWidget *howto_value;
	GtkWidget *url1_label;
	GtkWidget *url1_button;
	GtkWidget *url2_label;
	GtkWidget *url2_button;
	GtkWidget *folder_label;
	GtkWidget *folder_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtAsrDialog, pt_asr_dialog, GTK_TYPE_WINDOW)


static void
name_entry_activate_cb (GtkEntry *entry,
                        gpointer  user_data)
{
	PtAsrDialog *dlg = PT_ASR_DIALOG (user_data);
	gchar const *name;

	name = gtk_editable_get_text (GTK_EDITABLE (entry));
	gtk_window_set_title (GTK_WINDOW (dlg), name);
	pt_config_set_name (dlg->priv->config, (void*)name);
}

static gboolean
have_string (gchar *str)
{
	return (str && g_strcmp0 (str, "") != 0);
}

static void
set_folder (PtAsrDialog *dlg,
            gchar       *folder)
{
	GtkLabel *folder_label = GTK_LABEL (dlg->priv->folder_label);
	GtkButton *folder_button = GTK_BUTTON (dlg->priv->folder_button);

	if (have_string (folder)) {
		gtk_label_set_text (folder_label, folder);
		gtk_button_set_label (folder_button, _("Change folder"));
	} else {
		gtk_label_set_text (folder_label, _("No"));
		gtk_button_set_label (folder_button, _("Select model folder"));
	}
}

static void
folder_dialog_response_cb (GtkDialog *dialog,
                           gint       response_id,
                           gpointer   user_data)
{
	PtAsrDialog *dlg = PT_ASR_DIALOG (user_data);
	GFile    *result;
	gchar    *path = NULL;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		result = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
		path = g_file_get_path (result);
		g_object_unref (result);
	}

	if (path) {
		pt_config_set_base_folder (dlg->priv->config, path);
		set_folder (dlg, path);
		g_free (path);
	}

	g_object_unref (dialog);
}

static void
folder_button_clicked_cb (GtkButton *widget,
                          gpointer   user_data)
{
	PtAsrDialog *dlg = PT_ASR_DIALOG (user_data);
	GtkFileChooserNative *dialog;
	const char    *home_path;
	gchar         *path = NULL;
	GFile	      *dir;

	dialog = gtk_file_chooser_native_new (
			_("Select Model Folder"),
			GTK_WINDOW (dlg),
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
			_("_Open"),
			_("_Cancel"));

	/* Set current folder or user’s home directory */
	path = pt_config_get_base_folder (dlg->priv->config);
	if (have_string (path)) {
		dir = g_file_new_for_path (path);
	} else {
		home_path = g_get_home_dir ();
		dir = g_file_new_for_path (home_path);
	}
	gtk_file_chooser_set_current_folder (
		GTK_FILE_CHOOSER (dialog), dir, NULL);

	g_signal_connect (dialog, "response", G_CALLBACK (folder_dialog_response_cb), dlg);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));

	g_object_unref (dir);
}

void
pt_asr_dialog_set_config (PtAsrDialog *dlg,
                          PtConfig    *config)
{
	gchar *name;
	gchar *str;
	GtkWidget *label;
	gchar *engine = NULL;

	dlg->priv->config = config;

	name = pt_config_get_name (config);
	gtk_window_set_title (GTK_WINDOW (dlg), name);
	gtk_editable_set_text (GTK_EDITABLE (dlg->priv->name_entry), name);

	g_signal_connect (dlg->priv->name_entry, "activate", G_CALLBACK (name_entry_activate_cb), dlg);

	str = pt_config_get_lang_name (config);
	gtk_label_set_text (GTK_LABEL (dlg->priv->lang_value), str);

	str = pt_config_get_plugin (config);
	if (g_strcmp0 (str, "parlasphinx") == 0)
		engine = _("CMU Pocketsphinx");

	if (g_strcmp0 (str, "ptdeepspeech") == 0)
		engine = _("Mozilla DeepSpeech");

	if (engine) {
		gtk_widget_set_visible (dlg->priv->plugin_label, FALSE);
		gtk_widget_hide (dlg->priv->plugin_value);
		gtk_label_set_text (GTK_LABEL (dlg->priv->engine_value), engine);
	} else {
		gtk_widget_hide (dlg->priv->engine_label);
		gtk_widget_hide (dlg->priv->engine_value);
		gtk_label_set_text (GTK_LABEL (dlg->priv->plugin_value), str);
	}

	str = pt_config_get_key (config, "Publisher");
	if (have_string (str)) {
		gtk_label_set_text (GTK_LABEL (dlg->priv->publisher_value), str);
	} else {
		gtk_widget_hide (dlg->priv->publisher_label);
		gtk_widget_hide (dlg->priv->publisher_value);
	}
	g_free (str);

	str = pt_config_get_key (config, "License");
	if (have_string (str)) {
		gtk_label_set_text (GTK_LABEL (dlg->priv->license_value), str);
	} else {
		gtk_widget_hide (dlg->priv->license_label);
		gtk_widget_hide (dlg->priv->license_value);
	}
	g_free (str);

	/* Installation */
	str = pt_config_get_key (config, "Howto");
	if (have_string (str)) {
		gtk_label_set_text (GTK_LABEL (dlg->priv->howto_value), str);
	} else {
		gtk_widget_hide (dlg->priv->howto_label);
		gtk_widget_hide (dlg->priv->howto_value);
	}
	g_free (str);

	str = pt_config_get_key (config, "URL1");
	if (have_string (str)) {
		gtk_link_button_set_uri (GTK_LINK_BUTTON (dlg->priv->url1_button),
			                 str);
		gtk_button_set_label (GTK_BUTTON (dlg->priv->url1_button), str);

		/* Ellipsize label: This works only after gtk_button_set_label() */
		label = gtk_widget_get_first_child (dlg->priv->url1_button);
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	} else {
		gtk_widget_hide (dlg->priv->url1_label);
		gtk_widget_hide (dlg->priv->url1_button);
	}
	g_free (str);

	str = pt_config_get_key (config, "URL2");
	if (have_string (str)) {
		gtk_link_button_set_uri (GTK_LINK_BUTTON (dlg->priv->url2_button),
			                 str);
		gtk_button_set_label (GTK_BUTTON (dlg->priv->url2_button), str);

		/* Ellipsize label: This works only after gtk_button_set_label() */
		label = gtk_widget_get_first_child (dlg->priv->url2_button);
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	} else {
		gtk_widget_hide (dlg->priv->url2_label);
		gtk_widget_hide (dlg->priv->url2_button);
	}
	g_free (str);

	str = pt_config_get_base_folder (config);
	set_folder (dlg, str);
	gtk_label_set_ellipsize (GTK_LABEL (dlg->priv->folder_label), PANGO_ELLIPSIZE_START);
}

static void
pt_asr_dialog_init (PtAsrDialog *dlg)
{
	dlg->priv = pt_asr_dialog_get_instance_private (dlg);
	gtk_widget_init_template (GTK_WIDGET (dlg));
	gtk_widget_add_css_class (GTK_WIDGET (dlg), "ptdialog");
}

static void
pt_asr_dialog_class_init (PtAsrDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/asr-dialog.ui");
	gtk_widget_class_bind_template_callback(widget_class, folder_button_clicked_cb);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, name_entry);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, lang_value);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, engine_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, engine_value);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, plugin_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, plugin_value);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, publisher_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, publisher_value);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, license_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, license_value);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, howto_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, howto_value);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, url1_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, url1_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, url2_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, url2_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, folder_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, folder_button);
}

PtAsrDialog *
pt_asr_dialog_new (GtkWindow *win)
{
	return g_object_new (PT_TYPE_ASR_DIALOG, "transient-for", win, NULL);
}
