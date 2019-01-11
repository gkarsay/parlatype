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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <pt-player.h>
#include <pt-asr-settings.h>
#include "pt-asr-assistant-helpers.h"
#include "pt-asr-assistant.h"


struct _PtAsrAssistantPrivate
{
	/* Pages */
	GtkWidget *intro;
	GtkWidget *models;
	GtkWidget *summary;

	/* Widgets */
	GtkWidget *lm_chooser;
	GtkWidget *lm_icon;
	GtkWidget *lm_message;
	GtkWidget *dict_chooser;
	GtkWidget *dict_icon;
	GtkWidget *dict_message;
	GtkWidget *hmm_chooser;
	GtkWidget *hmm_icon;
	GtkWidget *hmm_message;
	GtkWidget *name_entry;

	/* Data */
	gchar     *lm_path;
	gchar     *dict_path;
	gchar     *hmm_path;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtAsrAssistant, pt_asr_assistant, GTK_TYPE_ASSISTANT)


static gchar*
get_path_for_uri (gchar *uri)
{
	GFile *file;
	gchar *path;

	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);
	g_object_unref (file);

	return path;
}

void
name_entry_changed_cb (GtkEntry       *entry,
                       PtAsrAssistant *self)
{
	gtk_assistant_set_page_complete (
			GTK_ASSISTANT (self),
			self->priv->summary,
			gtk_entry_get_text_length (entry) > 0);
}

static void
check_models_complete (PtAsrAssistant *self)
{
	gtk_assistant_set_page_complete (
			GTK_ASSISTANT (self),
			self->priv->models,
			self->priv->lm_path && self->priv->dict_path && self->priv->hmm_path);
}

void
lm_chooser_file_set_cb (GtkFileChooserButton *button,
                        PtAsrAssistant       *self)
{
	gchar *lm_uri;

	if (self->priv->lm_path) {
		g_free (self->priv->lm_path);
		self->priv->lm_path = NULL;
	}

	lm_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
	self->priv->lm_path = get_path_for_uri (lm_uri);
	g_free (lm_uri);

	check_models_complete (self);
}

void
dict_chooser_file_set_cb (GtkFileChooserButton *button,
                          PtAsrAssistant       *self)
{
	gchar *dict_uri;

	if (self->priv->dict_path) {
		g_free (self->priv->dict_path);
		self->priv->dict_path = NULL;
	}

	dict_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
	self->priv->dict_path = get_path_for_uri (dict_uri);
	g_free (dict_uri);

	check_models_complete (self);
}

static gboolean
check_hmm_folder (gchar *folder_uri)
{
	GError          *error = NULL;
	gchar           *folder_path;
	GFile           *folder;
	GFileEnumerator *files;
	gint             hits = 0;	

	folder = g_file_new_for_uri (folder_uri);
	folder_path = g_file_get_path (folder);
	files = g_file_enumerate_children (folder,
                                           G_FILE_ATTRIBUTE_STANDARD_NAME,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           NULL,	/* cancellable */
                                           &error);

	while (TRUE) {
		GFileInfo *info;
		if (!g_file_enumerator_iterate (files, &info, NULL, NULL, NULL)) /* GFile, cancellable & error */
			break;
		if (!info)
			break;
		const char *name = g_file_info_get_name (info);
		if (g_strcmp0 (name, "feat.params") == 0
		    || g_strcmp0 (name, "feature_transform") == 0
		    || g_strcmp0 (name, "mdef") == 0
		    || g_strcmp0 (name, "means") == 0
		    || g_strcmp0 (name, "mixture_weights") == 0
		    || g_strcmp0 (name, "noisedict") == 0
		    || g_strcmp0 (name, "sendump") == 0
		    || g_strcmp0 (name, "transition_matrices") == 0
		    || g_strcmp0 (name, "variances") == 0) {
			hits++;
			continue;
		}
	}

	g_object_unref (folder);
	g_object_unref (files);
	g_free (folder_path);
	
	return (hits > 4);
}

void
hmm_chooser_file_set_cb (GtkFileChooserButton *button,
                         PtAsrAssistant       *self)
{
	gchar *hmm_uri;

	if (self->priv->hmm_path) {
		g_free (self->priv->hmm_path);
		self->priv->hmm_path = NULL;
	}

	hmm_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
	gtk_image_clear (GTK_IMAGE (self->priv->hmm_icon));
	
	if (check_hmm_folder (hmm_uri)) {
		self->priv->hmm_path = get_path_for_uri (hmm_uri);
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->hmm_icon),
		                              "gtk-apply",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_widget_hide (self->priv->hmm_message);
	} else {
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->hmm_icon),
		                              "gtk-dialog-error",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_label_set_text (GTK_LABEL (self->priv->hmm_message),
		               _("The folder contains no acoustic model."));
		gtk_widget_show (self->priv->hmm_message);
	}

	g_free (hmm_uri);
	check_models_complete (self);
}

static void
error_message (PtAsrAssistant *self,
               const gchar    *message,
               const gchar    *secondary_message)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (
			GTK_WINDOW (self),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s", message);

	if (secondary_message)
		gtk_message_dialog_format_secondary_text (
				GTK_MESSAGE_DIALOG (dialog),
		                "%s", secondary_message);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
recursive_search_finished (PtAsrAssistant *self,
                           GAsyncResult   *res,
                           gpointer       *data)
{
	SearchResult *r;
	GError       *error = NULL;
	gchar        *folder_uri;
	gchar        *lm_uri;
	gchar        *dict_uri;
	gchar        *hmm_uri;
	
	folder_uri = (gchar*)data;
	r = _pt_asr_assistant_recursive_search_finish (self, res, &error);
	if (error) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			error_message (self,
			               _("Timed out"),
			               _("Scanning the folder took too long. Please make sure to open a folder with model data only."));
		} else {
			error_message (self,
			               _("Scanning the folder failed"),
			               error->message);
		}
		g_free (folder_uri);
		return;
	}

	if (r->lms->len == 0 && r->dicts->len == 0 && r->hmms->len == 0) {
		error_message (self,
		               _("Nothing found"),
		               _("This folder doesn't contain a suitable model."));
		search_result_free (r);
		g_free (folder_uri);
		return;
	}

	/* At least one item found, continue and resolve missing on next page */
	gtk_assistant_set_page_complete (
			GTK_ASSISTANT (self),
			self->priv->intro, TRUE);

	/* Setup next page, model page */

	lm_uri = g_strdup (folder_uri);
	dict_uri = g_strdup (folder_uri);
	hmm_uri = g_strdup (folder_uri);

	gtk_widget_show (self->priv->lm_message);
	gtk_widget_show (self->priv->dict_message);
	gtk_widget_show (self->priv->hmm_message);

	gtk_image_clear (GTK_IMAGE (self->priv->lm_icon));
	gtk_image_clear (GTK_IMAGE (self->priv->dict_icon));
	gtk_image_clear (GTK_IMAGE (self->priv->hmm_icon));

	if (r->lms->len == 0) {
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->lm_icon),
		                              "gtk-dialog-error",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_label_set_text (GTK_LABEL (self->priv->lm_message),
		               _("The folder contains no language model."));
	}

	if (r->lms->len == 1) {
		self->priv->lm_path = g_file_get_path (g_ptr_array_index (r->lms, 0));
		lm_uri = g_file_get_uri (g_ptr_array_index (r->lms, 0));
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->lm_icon),
		                              "gtk-apply",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_widget_hide (self->priv->lm_message);
	}

	if (r->lms->len > 1) {
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->lm_icon),
		                              "gtk-dialog-warning",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_label_set_text (GTK_LABEL (self->priv->lm_message),
		               _("The folder contains several language models. Please select one."));
	}

	if (r->dicts->len == 0) {
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->dict_icon),
		                              "gtk-dialog-error",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_label_set_text (GTK_LABEL (self->priv->dict_message),
		               _("The folder contains no dictionary."));
	}

	if (r->dicts->len == 1) {
		self->priv->dict_path = g_file_get_path (g_ptr_array_index (r->dicts, 0));
		dict_uri = g_file_get_uri (g_ptr_array_index (r->dicts, 0));
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->dict_icon),
		                              "gtk-apply",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_widget_hide (self->priv->dict_message);
	}

	if (r->dicts->len > 1) {
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->dict_icon),
		                              "gtk-dialog-warning",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_label_set_text (GTK_LABEL (self->priv->dict_message),
		               _("The folder contains several dictionaries. Please select one."));
	}
	if (r->hmms->len == 0) {
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->hmm_icon),
		                              "gtk-dialog-error",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_label_set_text (GTK_LABEL (self->priv->hmm_message),
		               _("The folder contains no acoustic model."));
	}

	if (r->hmms->len == 1) {
		self->priv->hmm_path = g_file_get_path (g_ptr_array_index (r->hmms, 0));
		hmm_uri = g_file_get_uri (g_ptr_array_index (r->hmms, 0));
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->hmm_icon),
		                              "gtk-apply",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_widget_hide (self->priv->hmm_message);
	}

	if (r->hmms->len > 1) {
		gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->hmm_icon),
		                              "gtk-dialog-warning",
		                              GTK_ICON_SIZE_BUTTON);
		gtk_label_set_text (GTK_LABEL (self->priv->hmm_message),
		               _("The folder contains several acoustic models. Please select one."));
	}

	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->lm_chooser), lm_uri);
	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->dict_chooser), dict_uri);
	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->hmm_chooser), hmm_uri);
	check_models_complete (self);

	g_free (folder_uri);
	g_free (lm_uri);
	g_free (dict_uri);
	g_free (hmm_uri);
	search_result_free (r);
}

void
folder_chooser_file_set_cb (GtkFileChooserButton *button,
                            PtAsrAssistant       *self)
{
	gchar *folder_uri;
	GFile *folder;

	/* Clear data */
	if (self->priv->lm_path) {
		g_free (self->priv->lm_path);
		self->priv->lm_path = NULL;
	}

	if (self->priv->dict_path) {
		g_free (self->priv->dict_path);
		self->priv->dict_path = NULL;
	}

	if (self->priv->hmm_path) {
		g_free (self->priv->hmm_path);
		self->priv->hmm_path = NULL;
	}

	gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->priv->intro, FALSE);

	folder_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
	if (!folder_uri) {
		return;
	}
	folder = g_file_new_for_uri (folder_uri);

	_pt_asr_assistant_recursive_search_async (self, folder,
			(GAsyncReadyCallback)recursive_search_finished,
			folder_uri);

	/* folder cleaned up in async, folder_uri in callback */
}

static gboolean
assistant_is_complete (PtAsrAssistant *self)
{
	return (gtk_assistant_get_page_complete (GTK_ASSISTANT (self), self->priv->summary));
}

gchar*
pt_asr_assistant_get_name (PtAsrAssistant *self)
{
	const gchar *name;
	gchar       *result;

	if (!assistant_is_complete (self))
		return NULL;

	name = gtk_entry_get_text (GTK_ENTRY (self->priv->name_entry));
	result = g_strdup (name);
	return result;
}

gchar*
pt_asr_assistant_save_config (PtAsrAssistant *self,
                              PtAsrSettings  *settings)
{
	gchar       *id;
	const gchar *entry_name;
	gchar       *name;

	if (!assistant_is_complete (self))
		return NULL;

	entry_name = gtk_entry_get_text (GTK_ENTRY (self->priv->name_entry));
	name = g_strdup (entry_name);
	id = pt_asr_settings_add_config (settings, name, "sphinx");
	pt_asr_settings_set_string (settings, id, "lm", self->priv->lm_path);
	pt_asr_settings_set_string (settings, id, "dict", self->priv->dict_path);
	pt_asr_settings_set_string (settings, id, "hmm", self->priv->hmm_path);

	g_free (name);
	return id;
}

static void
pt_asr_assistant_init (PtAsrAssistant *self)
{
	self->priv = pt_asr_assistant_get_instance_private (self);
	
	self->priv->lm_path = NULL;
	self->priv->dict_path = NULL;
	self->priv->hmm_path = NULL;

	GtkBuilder    *builder;
	builder = gtk_builder_new_from_resource ("/com/github/gkarsay/parlatype/asr-assistant.ui");

	self->priv->intro = GTK_WIDGET (gtk_builder_get_object (builder, "intro"));
	gtk_assistant_append_page (GTK_ASSISTANT (self), self->priv->intro);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->priv->intro, _("Download model"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->priv->intro, GTK_ASSISTANT_PAGE_INTRO);
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->priv->intro, FALSE);
	gtk_widget_show_all (self->priv->intro);

	self->priv->models = GTK_WIDGET (gtk_builder_get_object (builder, "models"));
	gtk_assistant_append_page (GTK_ASSISTANT (self), self->priv->models);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->priv->models, _("Confirm model"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->priv->models, GTK_ASSISTANT_PAGE_CONFIRM);
	self->priv->lm_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "lm_chooser"));
	self->priv->dict_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "dict_chooser"));
	self->priv->hmm_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "hmm_chooser"));
	self->priv->lm_icon = GTK_WIDGET (gtk_builder_get_object (builder, "lm_icon"));
	self->priv->dict_icon = GTK_WIDGET (gtk_builder_get_object (builder, "dict_icon"));
	self->priv->hmm_icon = GTK_WIDGET (gtk_builder_get_object (builder, "hmm_icon"));
	self->priv->lm_message = GTK_WIDGET (gtk_builder_get_object (builder, "lm_message"));
	self->priv->dict_message = GTK_WIDGET (gtk_builder_get_object (builder, "dict_message"));
	self->priv->hmm_message = GTK_WIDGET (gtk_builder_get_object (builder, "hmm_message"));
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->priv->models, FALSE);
	gtk_widget_show_all (self->priv->models);

	self->priv->summary = GTK_WIDGET (gtk_builder_get_object (builder, "summary"));
	gtk_assistant_append_page (GTK_ASSISTANT (self), self->priv->summary);
	gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->priv->summary, _("Name configuration"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->priv->summary, GTK_ASSISTANT_PAGE_SUMMARY);
	self->priv->name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->priv->summary, FALSE);
	gtk_widget_show_all (self->priv->summary);

	gtk_window_set_default_size (GTK_WINDOW (self), 500, -1);
	gtk_builder_connect_signals (builder, self);
	g_object_unref (builder);
}

static void
pt_asr_assistant_finalize (GObject *object)
{
	PtAsrAssistant *self = PT_ASR_ASSISTANT (object);

	g_free (self->priv->lm_path);
	g_free (self->priv->dict_path);
	g_free (self->priv->hmm_path);

	G_OBJECT_CLASS (pt_asr_assistant_parent_class)->finalize (object);
}

static void
pt_asr_assistant_class_init (PtAsrAssistantClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = pt_asr_assistant_finalize;
}

PtAsrAssistant *
pt_asr_assistant_new (GtkWindow *win)
{
	return g_object_new (
			PT_TYPE_ASR_ASSISTANT,
			"use-header-bar", TRUE,
			"transient-for", win,
			NULL);
}
