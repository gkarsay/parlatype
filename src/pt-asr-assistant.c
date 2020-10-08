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
#include "pt-asr-settings.h"
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
	GtkWidget *lm_combo;
	GtkWidget *lm_message;
	GtkWidget *lm_heading;
	GtkWidget *dict_chooser;
	GtkWidget *dict_combo;
	GtkWidget *dict_message;
	GtkWidget *dict_heading;
	GtkWidget *hmm_chooser;
	GtkWidget *hmm_combo;
	GtkWidget *hmm_message;
	GtkWidget *hmm_heading;
	GtkWidget *name_entry;

	GtkListStore *lm_list;
	GtkListStore *dict_list;
	GtkListStore *hmm_list;

	/* Data */
	gchar *lm_path;
	gchar *dict_path;
	gchar *hmm_path;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtAsrAssistant, pt_asr_assistant, GTK_TYPE_ASSISTANT)

static void folder_chooser_file_set_cb (GtkFileChooserButton *button, PtAsrAssistant *self);


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

static void
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

static void
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

static void
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
                                           &error);	/* TODO process error */

	while (TRUE) {
		GFileInfo *info;
		if (!g_file_enumerator_iterate (files, &info, NULL, NULL, NULL)) /* GFile, cancellable & TODO error */
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

static void
hmm_chooser_file_set_cb (GtkFileChooserButton *button,
                         PtAsrAssistant       *self)
{
	gchar *hmm_uri;

	if (self->priv->hmm_path) {
		g_free (self->priv->hmm_path);
		self->priv->hmm_path = NULL;
	}

	hmm_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));

	if (check_hmm_folder (hmm_uri)) {
		self->priv->hmm_path = get_path_for_uri (hmm_uri);
		gtk_widget_hide (self->priv->hmm_message);
	} else {
		gtk_label_set_text (GTK_LABEL (self->priv->hmm_message),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("The folder contains no acoustic model."));
		gtk_widget_show (self->priv->hmm_message);
	}

	g_free (hmm_uri);
	check_models_complete (self);
}

static void
lm_combo_changed_cb (GtkComboBox    *combo,
                     PtAsrAssistant *self)
{
	GtkTreeIter iter;

	/* This is also called when the ListStore is cleared */
	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->lm_list), &iter))
		return;

	if (gtk_combo_box_get_active (combo) == 0) {
		g_free (self->priv->lm_path);
		self->priv->lm_path = NULL;
	} else {
		gtk_combo_box_get_active_iter (combo, &iter);
		gtk_tree_model_get (GTK_TREE_MODEL (self->priv->lm_list), &iter, 1, &self->priv->lm_path, -1);
	}

	check_models_complete (self);
}

static void
dict_combo_changed_cb (GtkComboBox    *combo,
                       PtAsrAssistant *self)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->dict_list), &iter))
		return;

	if (gtk_combo_box_get_active (combo) == 0) {
		g_free (self->priv->dict_path);
		self->priv->dict_path = NULL;
	} else {
		gtk_combo_box_get_active_iter (combo, &iter);
		gtk_tree_model_get (GTK_TREE_MODEL (self->priv->dict_list), &iter, 1, &self->priv->dict_path, -1);
	}

	check_models_complete (self);
}

static void
hmm_combo_changed_cb (GtkComboBox    *combo,
                      PtAsrAssistant *self)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->hmm_list), &iter))
		return;

	if (gtk_combo_box_get_active (combo) == 0) {
		g_free (self->priv->hmm_path);
		self->priv->hmm_path = NULL;
	} else {
		gtk_combo_box_get_active_iter (combo, &iter);
		gtk_tree_model_get (GTK_TREE_MODEL (self->priv->hmm_list), &iter, 1, &self->priv->hmm_path, -1);
	}

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

	g_signal_connect_swapped (dialog, "response",
	                          G_CALLBACK (gtk_widget_destroy), dialog);

	gtk_widget_show_all (dialog);
}

static void
fill_list (GtkListStore *list,
           GPtrArray    *array,
           gchar        *base_uri)
{
	GtkTreeIter  iter;
	guint        i;
	GFile       *base;
	gchar       *relative_path;
	gchar       *full_path;

	base = g_file_new_for_uri (base_uri);

	gtk_list_store_append (list, &iter);
	/* Translators: This feature is disabled by default, its translation has low priority. */
	gtk_list_store_set (list, &iter, 0, _("Please select …"), -1);

	for (i = 0; i < array->len; i++) {
		gtk_list_store_append (list, &iter);
		relative_path = g_file_get_relative_path (base, g_ptr_array_index (array, i));
		full_path = g_file_get_path (g_ptr_array_index (array, i));
		gtk_list_store_set (list, &iter, 0, relative_path, 1, full_path, -1);
		g_free (relative_path);
		g_free (full_path);
	}

	g_object_unref (base);
}

static void
add_model_page (PtAsrAssistant *self)
{
	if (gtk_assistant_get_n_pages (GTK_ASSISTANT (self)) != 2)
		return;

	GtkBuilder *builder;
	builder = gtk_builder_new_from_resource ("/org/parlatype/parlatype/asr-assistant.ui");

	self->priv->models = GTK_WIDGET (gtk_builder_get_object (builder, "models"));
	self->priv->lm_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "lm_chooser"));
	self->priv->dict_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "dict_chooser"));
	self->priv->hmm_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "hmm_chooser"));
	self->priv->lm_combo = GTK_WIDGET (gtk_builder_get_object (builder, "lm_combo"));
	self->priv->dict_combo = GTK_WIDGET (gtk_builder_get_object (builder, "dict_combo"));
	self->priv->hmm_combo = GTK_WIDGET (gtk_builder_get_object (builder, "hmm_combo"));
	self->priv->lm_message = GTK_WIDGET (gtk_builder_get_object (builder, "lm_message"));
	self->priv->dict_message = GTK_WIDGET (gtk_builder_get_object (builder, "dict_message"));
	self->priv->hmm_message = GTK_WIDGET (gtk_builder_get_object (builder, "hmm_message"));
	self->priv->lm_heading = GTK_WIDGET (gtk_builder_get_object (builder, "lm_heading"));
	self->priv->dict_heading = GTK_WIDGET (gtk_builder_get_object (builder, "dict_heading"));
	self->priv->hmm_heading = GTK_WIDGET (gtk_builder_get_object (builder, "hmm_heading"));

	gtk_assistant_insert_page (GTK_ASSISTANT (self), self->priv->models, 1);
	/* Translators: This feature is disabled by default, its translation has low priority. */
	gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->priv->models, _("Missing parameters"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->priv->models, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->priv->models, FALSE);
	gtk_widget_show_all (self->priv->models);

	gtk_combo_box_set_model (GTK_COMBO_BOX (self->priv->lm_combo), GTK_TREE_MODEL (self->priv->lm_list));
	gtk_combo_box_set_model (GTK_COMBO_BOX (self->priv->dict_combo), GTK_TREE_MODEL (self->priv->dict_list));
	gtk_combo_box_set_model (GTK_COMBO_BOX (self->priv->hmm_combo), GTK_TREE_MODEL (self->priv->hmm_list));

	gtk_builder_add_callback_symbols (builder,
			"folder_chooser_file_set_cb", G_CALLBACK (folder_chooser_file_set_cb),
			"hmm_combo_changed_cb", G_CALLBACK (hmm_combo_changed_cb),
			"dict_combo_changed_cb", G_CALLBACK (dict_combo_changed_cb),
			"lm_combo_changed_cb", G_CALLBACK (lm_combo_changed_cb),
			"hmm_chooser_file_set_cb", G_CALLBACK (hmm_chooser_file_set_cb),
			"dict_chooser_file_set_cb", G_CALLBACK (dict_chooser_file_set_cb),
			"lm_chooser_file_set_cb", G_CALLBACK (lm_chooser_file_set_cb),
			"name_entry_changed_cb", G_CALLBACK (name_entry_changed_cb),
			NULL);
	gtk_builder_connect_signals (builder, self);
	g_object_unref (builder);
}

static void
remove_model_page (PtAsrAssistant *self)
{
	if (gtk_assistant_get_n_pages (GTK_ASSISTANT (self)) != 3)
		return;

	gtk_assistant_remove_page (GTK_ASSISTANT (self), 1);
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
	gboolean      nothing_found;
	gboolean      complete;

	folder_uri = (gchar*)data;
	r = _pt_asr_assistant_recursive_search_finish (self, res, &error);
	remove_model_page (self);

	if (error) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			error_message (self,
	/* Translators: This feature is disabled by default, its translation has low priority. */
			               _("Timed out"),
	/* Translators: This feature is disabled by default, its translation has low priority. */
			               _("Scanning the folder took too long. Please make sure to open a folder with model data only."));
		} else {
			error_message (self,
	/* Translators: This feature is disabled by default, its translation has low priority. */
			               _("Scanning the folder failed"),
			               error->message);
		}
		g_clear_error (&error);
		g_free (folder_uri);
		gtk_assistant_set_page_complete (GTK_ASSISTANT (self),
				self->priv->intro, FALSE);
		return;
	}

	nothing_found = (r->lms->len == 0 && r->dicts->len == 0 && r->hmms->len == 0);
	complete      = (r->lms->len == 1 && r->dicts->len == 1 && r->hmms->len == 1);

	if (nothing_found) {
		error_message (self,
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("Nothing found"),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("This folder doesn’t contain a suitable model."));
		search_result_free (r);
		g_free (folder_uri);
		gtk_assistant_set_page_complete (GTK_ASSISTANT (self),
				self->priv->intro, FALSE);
		return;
	}

	gtk_assistant_set_page_complete (GTK_ASSISTANT (self),
			self->priv->intro, TRUE);

	if (r->lms->len == 1)
		self->priv->lm_path   = g_file_get_path (g_ptr_array_index (r->lms, 0));
	if (r->dicts->len == 1)
		self->priv->dict_path = g_file_get_path (g_ptr_array_index (r->dicts, 0));
	if (r->hmms->len == 1)
		self->priv->hmm_path  = g_file_get_path (g_ptr_array_index (r->hmms, 0));

	if (complete) {
		search_result_free (r);
		g_free (folder_uri);
		return;
	}

	/* Setup model page */

	add_model_page (self);

	lm_uri = g_strdup (folder_uri);
	dict_uri = g_strdup (folder_uri);
	hmm_uri = g_strdup (folder_uri);

	gtk_widget_hide (self->priv->lm_combo);
	gtk_widget_hide (self->priv->dict_combo);
	gtk_widget_hide (self->priv->hmm_combo);

	gtk_widget_hide (self->priv->lm_chooser);
	gtk_widget_hide (self->priv->dict_chooser);
	gtk_widget_hide (self->priv->hmm_chooser);

	gtk_widget_hide (self->priv->lm_heading);
	gtk_widget_hide (self->priv->dict_heading);
	gtk_widget_hide (self->priv->hmm_heading);

	gtk_widget_hide (self->priv->lm_message);
	gtk_widget_hide (self->priv->dict_message);
	gtk_widget_hide (self->priv->hmm_message);

	if (r->lms->len == 0) {
		gtk_label_set_text (GTK_LABEL (self->priv->lm_message),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("The folder contains no language model. Please choose a language model from another folder."));
		gtk_widget_show (self->priv->lm_heading);
		gtk_widget_show (self->priv->lm_message);
		gtk_widget_show (self->priv->lm_chooser);
	}

	if (r->lms->len > 1) {
		gtk_label_set_text (GTK_LABEL (self->priv->lm_message),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("The folder contains several language models."));
		fill_list (self->priv->lm_list, r->lms, folder_uri);
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->lm_combo), 0);
		gtk_widget_show (self->priv->lm_heading);
		gtk_widget_show (self->priv->lm_message);
		gtk_widget_show (self->priv->lm_combo);
	}

	if (r->dicts->len == 0) {
		gtk_label_set_text (GTK_LABEL (self->priv->dict_message),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("The folder contains no dictionary. Please choose a dictionary from another folder."));
		gtk_widget_show (self->priv->dict_heading);
		gtk_widget_show (self->priv->dict_message);
		gtk_widget_show (self->priv->dict_chooser);
	}

	if (r->dicts->len > 1) {
		gtk_label_set_text (GTK_LABEL (self->priv->dict_message),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("The folder contains several dictionaries."));
		fill_list (self->priv->dict_list, r->dicts, folder_uri);
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->dict_combo), 0);
		gtk_widget_show (self->priv->dict_heading);
		gtk_widget_show (self->priv->dict_message);
		gtk_widget_show (self->priv->dict_combo);
	}
	if (r->hmms->len == 0) {
		gtk_label_set_text (GTK_LABEL (self->priv->hmm_message),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("The folder contains no acoustic model. Please choose an acoustic model from another folder."));
		gtk_widget_show (self->priv->hmm_heading);
		gtk_widget_show (self->priv->hmm_message);
		gtk_widget_show (self->priv->hmm_chooser);
	}

	if (r->hmms->len > 1) {
		gtk_label_set_text (GTK_LABEL (self->priv->hmm_message),
	/* Translators: This feature is disabled by default, its translation has low priority. */
		               _("The folder contains several acoustic models."));
		fill_list (self->priv->hmm_list, r->hmms, folder_uri);
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->hmm_combo), 0);
		gtk_widget_show (self->priv->hmm_heading);
		gtk_widget_show (self->priv->hmm_message);
		gtk_widget_show (self->priv->hmm_combo);
	}

	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->lm_chooser), lm_uri);
	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->dict_chooser), dict_uri);
	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->hmm_chooser), hmm_uri);

	g_free (folder_uri);
	g_free (lm_uri);
	g_free (dict_uri);
	g_free (hmm_uri);
	search_result_free (r);
}

static void
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

	gtk_list_store_clear (self->priv->lm_list);
	gtk_list_store_clear (self->priv->dict_list);
	gtk_list_store_clear (self->priv->hmm_list);

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

	GtkBuilder *builder;
	builder = gtk_builder_new_from_resource ("/org/parlatype/parlatype/asr-assistant.ui");

	self->priv->intro = GTK_WIDGET (gtk_builder_get_object (builder, "intro"));
	gtk_assistant_append_page (GTK_ASSISTANT (self), self->priv->intro);
	/* Translators: This feature is disabled by default, its translation has low priority. */
	gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->priv->intro, _("Download model"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->priv->intro, GTK_ASSISTANT_PAGE_INTRO);
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->priv->intro, FALSE);
	gtk_widget_show_all (self->priv->intro);

	self->priv->summary = GTK_WIDGET (gtk_builder_get_object (builder, "summary"));
	gtk_assistant_append_page (GTK_ASSISTANT (self), self->priv->summary);
	/* Translators: This feature is disabled by default, its translation has low priority. */
	gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->priv->summary, _("Finish configuration"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->priv->summary, GTK_ASSISTANT_PAGE_CONFIRM);
	self->priv->name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
	gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->priv->summary, FALSE);
	gtk_widget_show_all (self->priv->summary);

	gtk_window_set_default_size (GTK_WINDOW (self), 500, -1);
	gtk_builder_add_callback_symbols (builder,
			"folder_chooser_file_set_cb", G_CALLBACK (folder_chooser_file_set_cb),
			"hmm_combo_changed_cb", G_CALLBACK (hmm_combo_changed_cb),
			"dict_combo_changed_cb", G_CALLBACK (dict_combo_changed_cb),
			"lm_combo_changed_cb", G_CALLBACK (lm_combo_changed_cb),
			"hmm_chooser_file_set_cb", G_CALLBACK (hmm_chooser_file_set_cb),
			"dict_chooser_file_set_cb", G_CALLBACK (dict_chooser_file_set_cb),
			"lm_chooser_file_set_cb", G_CALLBACK (lm_chooser_file_set_cb),
			"name_entry_changed_cb", G_CALLBACK (name_entry_changed_cb),
			NULL);
	gtk_builder_connect_signals (builder, self);
	g_object_unref (builder);

	self->priv->lm_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	self->priv->dict_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	self->priv->hmm_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
}

static void
pt_asr_assistant_finalize (GObject *object)
{
	PtAsrAssistant *self = PT_ASR_ASSISTANT (object);

	g_clear_object (&self->priv->lm_list);
	g_clear_object (&self->priv->dict_list);
	g_clear_object (&self->priv->hmm_list);
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
