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
#include <pt-app.h>
#include <pt-player.h>
#include <pt-config.h>
#include "pt-config-row.h"
#include "pt-prefs-asr.h"


struct _PtPrefsAsrPrivate
{
	GSettings *editor;
	PtPlayer  *player;

	GFile     *config_folder;

	GtkWidget *asr_list;

	GtkWidget *asr_initial_box;
	GtkWidget *asr_ready_box;
	GtkWidget *asr_error_box;
	GtkWidget *asr_error_icon;
	GtkWidget *asr_error_label;
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
	int       num_loadable = 0;
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
				if (pt_player_config_is_loadable (page->priv->player, config)) {
					num_loadable++;
					row = pt_config_row_new (config);
					gtk_widget_show (GTK_WIDGET (row));
					gtk_container_add (asr_list, GTK_WIDGET (row));
					if (active_asr && g_strcmp0 (active_asr, path) == 0) {
						pt_config_row_set_active (row, TRUE);
						active_found = TRUE;
					}
					g_signal_connect (row, "notify::active", G_CALLBACK (config_activated), page);
				}
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

	if (error || (num_valid > 0 && num_loadable == 0)) {
		gtk_widget_hide (page->priv->asr_initial_box);
		gtk_widget_hide (page->priv->asr_ready_box);
		gtk_widget_show (page->priv->asr_error_box);
		if (error) {
			gtk_label_set_text (GTK_LABEL (page->priv->asr_error_label),
			                    _("Failed to read personal configuration files"));
			g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			                  "MESSAGE", "%s", error->message);
			g_error_free (error);
		} else {
			gtk_label_set_text (GTK_LABEL (page->priv->asr_error_label),
			                    _("There are no configurations available"));
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
		g_error_free (error);
	}
}

static void
pt_prefs_asr_init (PtPrefsAsr *page)
{
	gchar *path;

	page->priv = pt_prefs_asr_get_instance_private (page);

	gtk_widget_init_template (GTK_WIDGET (page));

	/* make sure config dir exists */
	path = g_build_path (G_DIR_SEPARATOR_S,
	                     g_get_user_config_dir (),
	                     PACKAGE, NULL);
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
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_error_box);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_initial_box);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_ready_box);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_error_icon);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_error_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtPrefsAsr, asr_list);
}

static void
pt_prefs_asr_set_editor (PtPrefsAsr *page,
                         GSettings  *editor)
{
	page->priv->editor = editor;
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
