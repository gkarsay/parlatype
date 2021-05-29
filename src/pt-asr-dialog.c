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
	GtkWidget *folder_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtAsrDialog, pt_asr_dialog, GTK_TYPE_WINDOW)


static void
name_entry_activate_cb (GtkEntry *entry,
                        gpointer  user_data)
{
	PtAsrDialog *dlg = PT_ASR_DIALOG (user_data);
	gchar const *name;

	name = gtk_entry_get_text (entry);
	gtk_window_set_title (GTK_WINDOW (dlg), name);
	pt_config_set_name (dlg->priv->config, (void*)name);
}

static gboolean
have_string (gchar *str)
{
	return (str && g_strcmp0 (str, "") != 0);
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
	gtk_entry_set_text (GTK_ENTRY (dlg->priv->name_entry), name);

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
		label = gtk_bin_get_child (GTK_BIN (dlg->priv->url1_button));
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
		label = gtk_bin_get_child (GTK_BIN (dlg->priv->url2_button));
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	} else {
		gtk_widget_hide (dlg->priv->url2_label);
		gtk_widget_hide (dlg->priv->url2_button);
	}
	g_free (str);

	str = pt_config_get_base_folder (config);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg->priv->folder_button),
	                                     str);
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
	gtk_widget_class_bind_template_child_private (widget_class, PtAsrDialog, folder_button);
}

PtAsrDialog *
pt_asr_dialog_new (GtkWindow *win)
{
	return g_object_new (PT_TYPE_ASR_DIALOG, "transient-for", win, NULL);
}
