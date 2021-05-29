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
#include "pt-app.h"
#include "pt-asr-dialog.h"
#include "pt-config-row.h"
#include "pt-preferences.h"
#include "pt-prefs-asr.h"


struct _PtPrefsAsrPrivate
{
	GSettings *editor;
	PtPlayer  *player;

	GFile     *config_folder;

	GtkWidget *filter_combo;
	GtkWidget *asr_list;
	GtkWidget *remove_button;
	GtkWidget *details_button;

	GtkWidget *asr_initial_box;
	GtkWidget *asr_ready_box;
	GtkWidget *asr_error_box;
	GtkWidget *asr_error_label;
	GtkWidget *no_result_box;

	int        filter;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtPrefsAsr, pt_prefs_asr, GTK_TYPE_BOX)

static void config_activated (GObject *object, GParamSpec *spec, gpointer user_data);

static void
block_rows (GtkWidget *widget,
            gpointer   user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);

	g_signal_handlers_block_by_func (widget, config_activated, page);
}

static void
unblock_rows (GtkWidget *widget,
              gpointer   user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);

	g_signal_handlers_unblock_by_func (widget, config_activated, page);
}

static void
set_active_row (GtkWidget *widget,
                gpointer   user_data)
{
	GtkWidget *active_widget = GTK_WIDGET (user_data);
	gboolean active;

	active = (widget == active_widget);

	pt_config_row_set_active (PT_CONFIG_ROW (widget), active);
}

static void
config_activated (GObject    *object,
                  GParamSpec *spec,
                  gpointer    user_data)
{
	PtPrefsAsr   *page = PT_PREFS_ASR (user_data);
	PtConfigRow  *row  = PT_CONFIG_ROW (object);
	GtkContainer *box  = GTK_CONTAINER (page->priv->asr_list);
	PtConfig *config;
	GFile    *file;
	gchar    *path;

	g_object_get (row, "config", &config, NULL);

	/* Only one row can be active, so set all others to inactive.
	 * Setting another row inactive would emit a signal and the whole
	 * procedure would start again, so block all signals for now. */
	gtk_container_foreach (box, block_rows, page);

	if (pt_config_row_get_active (row)) {
		file = pt_config_get_file (config);
		path = g_file_get_path (file);
		g_settings_set_string (page->priv->editor, "asr-config", path);
		g_free (path);
		gtk_container_foreach (box, set_active_row, row);
	} else {
		g_settings_set_string (page->priv->editor, "asr-config", "");
		gtk_container_foreach (box, set_active_row, NULL);
	}

	gtk_container_foreach (box, unblock_rows, page);

	g_object_unref (config);
}

static void
asr_list_row_activated (GtkListBox    *box,
                        GtkListBoxRow *box_row,
                        gpointer       user_data)
{
	PtConfigRow *row = PT_CONFIG_ROW (box_row);
	PtConfig    *config;

	g_object_get (row, "config", &config, NULL);

	if (pt_config_row_get_active (row)) {
		pt_config_row_set_active (row, FALSE);
	} else {
		if (pt_config_is_installed (config)) {
			pt_config_row_set_active (row, TRUE);
		}
	}

	g_object_unref (config);
}

static void
asr_list_changed_cb (GtkContainer *asr_list,
                     GtkWidget    *row,
                     gpointer      user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);
	GList *children;

	children = gtk_container_get_children (asr_list);
	if (children) {
		gtk_widget_show (page->priv->asr_ready_box);
		gtk_widget_hide (page->priv->asr_initial_box);
	} else {
		gtk_widget_hide (page->priv->asr_ready_box);
		gtk_widget_show (page->priv->asr_initial_box);
	}

	g_list_free (children);
}

static int
sort_asr_list (GtkListBoxRow *row1,
               GtkListBoxRow *row2,
               gpointer       user_data)
{
	/* 1st sort order: 1) Installed
	 *                 2) Supported
	 *                 3) Not supported */

	int left = 0;
	int right = 0;
	int comp;
	PtConfig *conf1, *conf2;
	gchar *str1, *str2;
	const gchar* const *langs;
	gboolean prefix1, prefix2;

	if (pt_config_row_get_supported (PT_CONFIG_ROW (row1)))
		left = 1;
	if (pt_config_row_is_installed (PT_CONFIG_ROW (row1)) && left == 1)
		left = 2;
	if (pt_config_row_get_active (PT_CONFIG_ROW (row1)))
		left = 3;

	if (pt_config_row_get_supported (PT_CONFIG_ROW (row2)))
		right = 1;
	if (pt_config_row_is_installed (PT_CONFIG_ROW (row2)) && right == 1)
		right = 2;
	if (pt_config_row_get_active (PT_CONFIG_ROW (row2)))
		right = 3;

	if (left > right)
		return -1;

	if (left < right)
		return 1;

	/* 2nd sort order: Own locale language before other languages */

	g_object_get (row1, "config", &conf1, NULL);
	g_object_get (row2, "config", &conf2, NULL);

	langs = g_get_language_names (); /* TODO: cache */
	if (langs[0]) {
		str1 = pt_config_get_lang_code (conf1);
		str2 = pt_config_get_lang_code (conf2);
		prefix1 = g_str_has_prefix (langs[0], str1);
		prefix2 = g_str_has_prefix (langs[0], str2);

		comp = 0;
		if (prefix1 && !prefix2)
			comp = -1;
		if (!prefix1 && prefix2)
			comp = 1;

		if (comp != 0) {
			g_object_unref (conf1);
			g_object_unref (conf2);
			return comp;
		}
	}

	/* 3rd sort order: Alphabetically by language name */

	str1 = pt_config_get_lang_name (conf1);
	str2 = pt_config_get_lang_name (conf2);

	comp = g_strcmp0 (str1, str2);
	if (comp != 0) {
		g_object_unref (conf1);
		g_object_unref (conf2);
		return comp;
	}

	/* 4th sort order: Alphabetically by name */

	str1 = pt_config_get_name (conf1);
	str2 = pt_config_get_name (conf2);

	comp = g_strcmp0 (str1, str2);
	g_object_unref (conf1);
	g_object_unref (conf2);
	return comp;
}

static gboolean
filter_asr_list (GtkListBoxRow *row,
                 gpointer       user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);
	gboolean    supported = TRUE;
	int         filter;

	filter = page->priv->filter;

	/* Filters: 0 ... installed
	 *          1 ... supported
	 *          2 ... all        */

	/* Filter installed and supported
	 * Theoretically a model can be installed, but not supported */
	if (filter <= 1)
		supported = pt_config_row_get_supported (PT_CONFIG_ROW (row));

	/* Filter installed */
	if (filter == 0) {
		if (!supported)
			return FALSE;
		else
			return pt_config_row_is_installed (PT_CONFIG_ROW (row));
	}

	/* Filter supported */
	if (filter == 1)
		return supported;

	/* All other rows are visible */
	return TRUE;
}

/*static void
add_config_row (PtPrefsAsr *page,
                PtConfig   *config,
		gboolean    active)
{
	PtConfigRow *row;

	row = pt_config_row_new (config);
	pt_config_row_set_supported (row, pt_player_config_is_loadable (page->priv->player, config));
	gtk_widget_show (GTK_WIDGET (row));
	gtk_container_add (page->priv->asr_list, GTK_WIDGET (row));
	if (active)
		pt_config_row_set_active (row, TRUE);
	g_signal_connect (row, "notify::active", G_CALLBACK (config_activated), page);
}*/

static void
asr_setup_config_box (PtPrefsAsr *page)
{
	GError   *error = NULL;
	PtConfig *config;
	gchar    *path;
	GFile    *file;
	GFileEnumerator *files;
	PtConfigRow *row;
	gchar    *active_asr;
	gboolean  active_found = FALSE;
	int       num_valid = 0;
	GtkContainer *asr_list = GTK_CONTAINER (page->priv->asr_list);

	active_asr = g_settings_get_string (page->priv->editor, "asr-config");
	if (g_strcmp0 (active_asr, "") == 0) {
		g_free (active_asr);
		active_asr = NULL;
	}

	files = g_file_enumerate_children (page->priv->config_folder,
	                                   G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                                   NULL,	/* cancellable */
	                                   &error);

	while (!error) {
		GFileInfo *info;
		if (!g_file_enumerator_iterate (files, &info, &file, NULL, &error))
			break;
		if (!info)
			break;

		path = g_file_get_path (file);
		if (g_str_has_suffix (path, ".asr")) {
			config = pt_config_new (file);
			if (pt_config_is_valid (config)) {
				num_valid++;
				row = pt_config_row_new (config);
				pt_config_row_set_supported (row, pt_player_config_is_loadable (page->priv->player, config));
				gtk_widget_show (GTK_WIDGET (row));
				gtk_container_add (asr_list, GTK_WIDGET (row));
				if (active_asr && g_strcmp0 (active_asr, path) == 0) {
					pt_config_row_set_active (row, TRUE);
					active_found = TRUE;
				}
				g_signal_connect (row, "notify::active", G_CALLBACK (config_activated), page);
			} else {
				g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
				                  "MESSAGE", "File %s is not valid", path);
			}
			g_object_unref (config);
		}
		g_free (path);
	}

	if (active_asr && !active_found)
		g_settings_set_string (page->priv->editor, "asr-config", "");

	g_clear_object (&files);
	g_free (active_asr);

	gtk_list_box_set_filter_func (GTK_LIST_BOX (asr_list),
                              filter_asr_list,
                              page,
                              NULL);

	gtk_list_box_set_sort_func (GTK_LIST_BOX (asr_list),
                              sort_asr_list,
                              NULL,
                              NULL);

	if (error || num_valid == 0) {
		gtk_widget_hide (page->priv->asr_ready_box);
		if (error) {
			gtk_widget_show (page->priv->asr_error_box);
			gtk_label_set_text (GTK_LABEL (page->priv->asr_error_label),
			                    _("Failed to read personal configuration files"));
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			                  "MESSAGE", "%s", error->message);
			g_error_free (error);
		} else {
			gtk_widget_show (page->priv->asr_initial_box);
		}
	} else {
		asr_list_changed_cb (asr_list, NULL, page);
	}
}

static gboolean
copy_asr_configs_finish (GObject       *source_object,
                         GAsyncResult  *res,
                         GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (res, source_object), FALSE);

	return g_task_propagate_boolean (G_TASK (res), error);

}

static void
copy_asr_configs_result (GObject       *source_object,
                         GAsyncResult  *res,
                         gpointer       user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);
	GError *error = NULL;

	if (copy_asr_configs_finish (source_object, res, &error)) {
		g_signal_handlers_block_by_func (page->priv->asr_list,
		                                 asr_list_changed_cb, page);
		asr_setup_config_box (page);
		g_signal_handlers_unblock_by_func (page->priv->asr_list,
		                                   asr_list_changed_cb, page);
	} else {
		gtk_widget_hide (page->priv->asr_initial_box);
		gtk_widget_hide (page->priv->asr_ready_box);
		gtk_widget_show (page->priv->asr_error_box);
		gtk_label_set_text (GTK_LABEL (page->priv->asr_error_label),
		                   _("Failed to copy shipped configuration files"));
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
				  "MESSAGE", "%s", error->message);
		g_error_free (error);
	}
}

static void
copy_asr_configs (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
	PtPrefsAsr *page = PT_PREFS_ASR (source_object);
	gchar    *basename;
	gchar    *dest_folder_path;
	gchar    *dest_path;
	GFile    *source;
	GFile    *dest;
	GFile    *sys_folder;
	GFileEnumerator *files;
	GError   *error = NULL;

	sys_folder = g_file_new_for_path (ASR_DIR); /* in config.h */
	dest_folder_path = g_file_get_path (page->priv->config_folder);

	files = g_file_enumerate_children (sys_folder,
	                                   G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
	                                   NULL,	/* cancellable */
	                                   &error);

	while (!error) {
		GFileInfo *info;
		if (!g_file_enumerator_iterate (files,
		                                &info,	/* no free necessary */
		                                &source,/* no free necessary */
		                                NULL,   /* cancellable       */
		                                &error))
			/* this is an error */
			break;

		if (!info)
			/* this is the end of enumeration */
			break;

		basename = g_file_get_basename (source);
		if (g_str_has_suffix (basename, ".asr")) {
			dest_path = g_build_path (G_DIR_SEPARATOR_S,
			                          dest_folder_path,
			                          basename, NULL);
			dest = g_file_new_for_path (dest_path);
			g_file_copy (source,
			             dest,
			             G_FILE_COPY_TARGET_DEFAULT_PERMS,
			             NULL, /* cancellable            */
			             NULL, /* progress callback      */
			             NULL, /* progress callback data */
			             &error);
			g_free (dest_path);
			g_object_unref (dest);

			if (error && g_error_matches (error, G_IO_ERROR,
			                              G_IO_ERROR_EXISTS)) {
				g_clear_error (&error);
			}
		}
		g_free (basename);
	}

	g_clear_object (&files);
	g_object_unref (sys_folder);
	g_free (dest_folder_path);

	if (error)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);
}

static void
initial_asr_button_clicked_cb (GtkButton  *button,
                               PtPrefsAsr *page)
{
	GTask *task;
	task = g_task_new (page,  /* source object */
	                   NULL, /* cancellable   */
	                   copy_asr_configs_result,
	                   page); /* user_data     */

	g_task_run_in_thread (task, copy_asr_configs);
	g_object_unref (task);
}

static void
file_delete_finished (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
	PtConfigRow *row = PT_CONFIG_ROW (user_data);
	GFile       *file  = G_FILE (source_object);
	GError      *error = NULL;
	GtkWidget   *dialog;
	GtkWidget   *parent;

	if (g_file_delete_finish (file, res, &error)) {
		if (pt_config_row_get_active (row))
			pt_config_row_set_active (row, FALSE);
		gtk_widget_destroy (GTK_WIDGET (row));
	} else {
		parent = gtk_widget_get_ancestor (GTK_WIDGET (row),
		                                  PT_TYPE_PREFERENCES_DIALOG);

		dialog = gtk_message_dialog_new (
				GTK_WINDOW (parent),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"Error deleting configuration");

		gtk_message_dialog_format_secondary_text (
				GTK_MESSAGE_DIALOG (dialog),
				"%s", error->message);

		g_signal_connect_swapped (dialog, "response",
		                          G_CALLBACK (gtk_widget_destroy), dialog);

		gtk_widget_show_all (dialog);
		g_error_free (error);
	}
}

static void
confirm_delete_response_cb (GtkDialog *dialog,
                            gint       response_id,
                            gpointer   user_data)
{
	PtConfigRow *row = PT_CONFIG_ROW (user_data);
	PtConfig    *config;
	GFile       *file;

	gtk_widget_destroy (GTK_WIDGET (dialog));
	if (response_id != GTK_RESPONSE_YES)
		return;

	g_object_get (row, "config", &config, NULL);
	file = pt_config_get_file (config);
	g_file_delete_async (file,
	                     G_PRIORITY_DEFAULT,
	                     NULL, /* cancellable */
	                     file_delete_finished,
	                     row);
}

static void
confirm_delete (PtConfigRow *row)
{
	PtConfig        *config;
	GtkWidget       *dialog;
	GtkWidget       *parent;
	GtkWidget       *yes_button;
	GtkStyleContext *context;
	gchar		*message;
	gchar		*secondary_message;
	gchar           *name;

	g_object_get (row, "config", &config, NULL);
	name = pt_config_get_name (config);
	parent = gtk_widget_get_ancestor (GTK_WIDGET (row), PT_TYPE_PREFERENCES_DIALOG);

	message = _("Delete Model Definition?");
	secondary_message = g_strdup_printf ("Do you really want to delete »%s«?", name);
	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   "%s", message);

	/* Add secondary message and buttons, mark Yes-Button as destructive */
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary_message);
	gtk_dialog_add_button (GTK_DIALOG (dialog), "_Cancel", GTK_RESPONSE_CANCEL);
	yes_button = gtk_dialog_add_button (GTK_DIALOG (dialog), "_Yes", GTK_RESPONSE_YES);
	context = gtk_widget_get_style_context (yes_button);
	gtk_style_context_add_class (context, "destructive-action");

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (confirm_delete_response_cb), row);

	gtk_widget_show_all (dialog);

	g_free (secondary_message);
}

static void
remove_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
	PtPrefsAsr  *page = PT_PREFS_ASR (user_data);
	PtConfigRow *row;

	row = PT_CONFIG_ROW (gtk_list_box_get_selected_row (GTK_LIST_BOX (page->priv->asr_list)));
	if (!row)
		return;

	confirm_delete (row);
}

static void
details_button_clicked_cb (GtkButton *button,
                           gpointer   user_data)
{
	PtPrefsAsr  *page = PT_PREFS_ASR (user_data);
	PtConfigRow *row;
	PtConfig    *config;

	row = PT_CONFIG_ROW (gtk_list_box_get_selected_row (GTK_LIST_BOX (page->priv->asr_list)));
	if (!row)
		return;

	g_object_get (row, "config", &config, NULL);
	GtkWidget *parent = gtk_widget_get_ancestor (GTK_WIDGET (button), PT_TYPE_PREFERENCES_DIALOG);
	PtAsrDialog *dlg = pt_asr_dialog_new (GTK_WINDOW (parent));
	gtk_widget_show_all (GTK_WIDGET (dlg));
	pt_asr_dialog_set_config (dlg, config);
}

static void
import_copy_ready_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
	PtPrefsAsr  *page = PT_PREFS_ASR (user_data);
	GFile       *file = G_FILE (source_object);
	GError      *error = NULL;
	gboolean     success;
	PtConfig    *config;
	PtConfigRow *row;
	GtkWidget   *parent;
	GtkWidget   *err_dialog;

	success = g_file_copy_finish (file, res, &error);

	if (success) {
		config = pt_config_new (file);
		row = pt_config_row_new (config);
		pt_config_row_set_supported (row, pt_player_config_is_loadable (page->priv->player, config));
		gtk_widget_show (GTK_WIDGET (row));
		gtk_container_add (GTK_CONTAINER (page->priv->asr_list), GTK_WIDGET (row));
		g_signal_connect (row, "notify::active", G_CALLBACK (config_activated), page);
	} else {
		parent = gtk_widget_get_ancestor (GTK_WIDGET (page),
		                                  PT_TYPE_PREFERENCES_DIALOG);

		err_dialog = gtk_message_dialog_new (
				GTK_WINDOW (parent),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("Error"));

		gtk_message_dialog_format_secondary_text (
				GTK_MESSAGE_DIALOG (err_dialog),
				"%s", error->message);

		g_signal_connect_swapped (err_dialog, "response",
		                          G_CALLBACK (gtk_widget_destroy), err_dialog);

		gtk_widget_show_all (err_dialog);
		g_error_free (error);
	}
}

static void
import_dialog_response_cb (GtkDialog *dialog,
                           gint       response_id,
                           gpointer   user_data)
{
	PtPrefsAsr  *page = PT_PREFS_ASR (user_data);
	gchar       *uri = NULL;
	GFile       *source;
	PtConfig    *config;
	GtkWidget   *parent;
	GtkWidget   *err_dialog;
	gchar       *dest_folder_path;
	gchar       *basename;
	gchar       *dest_path;
	GFile       *dest;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
	}

	g_object_unref (dialog);

	if (!uri)
		return;

	source = g_file_new_for_uri (uri);
	g_free (uri);

	config = pt_config_new (source);
	g_object_unref (source);

	if (!pt_config_is_valid (config)) {
		parent = gtk_widget_get_ancestor (GTK_WIDGET (page),
		                                  PT_TYPE_PREFERENCES_DIALOG);

		err_dialog = gtk_message_dialog_new (
				GTK_WINDOW (parent),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("Error"));

		gtk_message_dialog_format_secondary_text (
				GTK_MESSAGE_DIALOG (err_dialog),
				_("The file is not a valid model definition."));

		g_signal_connect_swapped (err_dialog, "response",
		                          G_CALLBACK (gtk_widget_destroy), err_dialog);

		gtk_widget_show_all (err_dialog);
		g_object_unref (config);
		return;
	}

	dest_folder_path = g_file_get_path (page->priv->config_folder);
	basename = g_file_get_basename (source);
	dest_path = g_build_path (G_DIR_SEPARATOR_S,
	                          dest_folder_path,
	                          basename, NULL);
	dest = g_file_new_for_path (dest_path);

	g_file_copy_async (
			source, dest,
			G_FILE_COPY_TARGET_DEFAULT_PERMS,
			G_PRIORITY_DEFAULT,
			NULL, /* cancellable */
			NULL, NULL, /* progress callback and data*/
			import_copy_ready_cb,
			page);

	g_object_unref (source);
	g_object_unref (dest);
	g_free (basename);
	g_free (dest_folder_path);
	g_free (dest_path);
}

static void
import_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
	PtPrefsAsr  *page = PT_PREFS_ASR (user_data);

	GtkFileChooserNative *dialog;
	GtkWidget     *parent;
	const char    *home_path;
	GFile	      *home;
	GtkFileFilter *filter_asr;
	GtkFileFilter *filter_all;

	parent = gtk_widget_get_ancestor (GTK_WIDGET (button), PT_TYPE_PREFERENCES_DIALOG);
	dialog = gtk_file_chooser_native_new (
			_("Import ASR Model Definition"),
			GTK_WINDOW (parent),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("_Open"),
			_("_Cancel"));

	/* Set current folder to the user’s home directory */
	home_path = g_get_home_dir ();
	home = g_file_new_for_path (home_path);
	gtk_file_chooser_set_current_folder_file (
		GTK_FILE_CHOOSER (dialog), home, NULL);

	filter_asr = gtk_file_filter_new ();
	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_asr, _("Parlatype Model definitions"));
	gtk_file_filter_set_name (filter_all, _("All files"));
	gtk_file_filter_add_pattern (filter_asr, "*.asr");
	gtk_file_filter_add_pattern (filter_all, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_asr);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter_asr);

	g_signal_connect (dialog, "response", G_CALLBACK (import_dialog_response_cb), page);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));

	g_object_unref (home);
}

static void
filter_combo_changed_cb (GtkComboBox *combo,
                         gpointer     user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);
	PtPrefsAsrPrivate *priv = page->priv;

	priv->filter = gtk_combo_box_get_active (combo);
	g_settings_set_int (priv->editor, "asr-filter", priv->filter);
	gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->asr_list));
}

static void
make_config_dir_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);
	GFile   *config_folder = G_FILE (source_object);
	GError  *error = NULL;
	gboolean success;

	success = g_file_make_directory_finish (config_folder, res, &error);

	if (success || (!success && g_error_matches (error, G_IO_ERROR,
	                                             G_IO_ERROR_EXISTS))) {
		gtk_widget_hide (page->priv->asr_error_box);
		asr_setup_config_box (page);
		g_signal_connect (page->priv->asr_list, "add",
		                  G_CALLBACK (asr_list_changed_cb), page);
		g_signal_connect (page->priv->asr_list, "remove",
		                  G_CALLBACK (asr_list_changed_cb), page);
	} else {
		gtk_widget_hide (page->priv->asr_initial_box);
		gtk_widget_hide (page->priv->asr_ready_box);
		gtk_widget_show (page->priv->asr_error_box);
		gtk_label_set_text (GTK_LABEL (page->priv->asr_error_label),
		                   _("Failed to create personal configuration folder"));
		g_settings_set_string (page->priv->editor, "asr-config", "");
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
		                  "MESSAGE", "%s", error->message);
	}

	g_clear_error (&error);
}

static void
asr_list_row_selected_cb (GtkListBox    *box,
                          GtkListBoxRow *row,
                          gpointer       user_data)
{
	PtPrefsAsr *page = PT_PREFS_ASR (user_data);
	gboolean selected;

	selected = (row != NULL);
	gtk_widget_set_sensitive (page->priv->remove_button, selected);
	gtk_widget_set_sensitive (page->priv->details_button, selected);
}

static void
pt_prefs_asr_init (PtPrefsAsr *page)
{
	gchar *path;

	page->priv = pt_prefs_asr_get_instance_private (page);

	gtk_widget_init_template (GTK_WIDGET (page));

	page->priv->filter = 2;
	gtk_list_box_set_placeholder (GTK_LIST_BOX (page->priv->asr_list), page->priv->no_result_box);

	/* make sure config dir exists */
	path = g_build_path (G_DIR_SEPARATOR_S,
	                     g_get_user_config_dir (),
	                     PACKAGE_NAME, NULL);

	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "config dir: %s", path);

	page->priv->config_folder = g_file_new_for_path (path);
	g_free (path);

	/* init is continued in make_config_dir_cb */
	g_file_make_directory_async (page->priv->config_folder,
	                             G_PRIORITY_DEFAULT,
	                             NULL, /* cancellable */
	                             make_config_dir_cb,
	                             page);
}

static void
pt_prefs_asr_dispose (GObject *object)
{
	PtPrefsAsr *page = PT_PREFS_ASR (object);

	g_clear_object (&page->priv->config_folder);

	G_OBJECT_CLASS (pt_prefs_asr_parent_class)->dispose (object);
}

static void
pt_prefs_asr_class_init (PtPrefsAsrClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = pt_prefs_asr_dispose;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/prefs-asr.ui");
	gtk_widget_class_bind_template_callback(widget_class, initial_asr_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, asr_list_row_activated);
	gtk_widget_class_bind_template_callback(widget_class, asr_list_row_selected_cb);
	gtk_widget_class_bind_template_callback(widget_class, filter_combo_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, details_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, remove_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, import_button_clicked_cb);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, filter_combo);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, remove_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, details_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_error_box);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_initial_box);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_ready_box);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_error_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_list);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, no_result_box);
}

static void
pt_prefs_asr_set_editor (PtPrefsAsr *page,
                         GSettings  *editor)
{
	PtPrefsAsrPrivate *priv = page->priv;

	priv->editor = editor;
	priv->filter = g_settings_get_int (editor, "asr-filter");
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->filter_combo), priv->filter);
}

static void
pt_prefs_asr_set_player (PtPrefsAsr *page,
                         PtPlayer   *player)
{
	page->priv->player = player;
}

GtkWidget*
pt_prefs_asr_new (GSettings *editor,
		  PtPlayer  *player)
{
	PtPrefsAsr *asr_page;

	asr_page = g_object_new (PT_TYPE_PREFS_ASR, NULL);
	pt_prefs_asr_set_editor (asr_page, editor);
	pt_prefs_asr_set_player (asr_page, player);

	return GTK_WIDGET (asr_page);
}
