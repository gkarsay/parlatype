/* Copyright (C) Gabor Karsay 2016, 2018, 2019 <gabor.karsay@gmx.at>
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
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#include <pt-app.h>
#include <pt-player.h>
#include "pt-asr-assistant.h"
#include "pt-asr-settings.h"
#include "pt-preferences.h"

#define EXAMPLE_TIME_SHORT 471123
#define EXAMPLE_TIME_LONG 4071123

static GtkWidget *preferences_dialog = NULL;

struct _PtPreferencesDialogPrivate
{
	GSettings *editor;
	PtPlayer  *player;
	GtkWidget *notebook;

	/* Waveform tab */
	GtkWidget *pps_scale;
	GtkWidget *ruler_check;
	GtkWidget *fixed_cursor_radio;

	/* Controls tab */
	GtkWidget *spin_pause;
	GtkWidget *spin_back;
	GtkWidget *spin_forward;
	GtkWidget *label_pause;
	GtkWidget *label_back;
	GtkWidget *label_forward;
	GtkWidget *repeat_all_checkbox;
	GtkWidget *repeat_selection_checkbox;

	/* Window tab */
	GtkWidget *size_check;
	GtkWidget *pos_check;
	GtkWidget *top_check;

	/* Timestamps tab */
	GtkWidget *precision_combo;
	GtkWidget *separator_combo;
	GtkWidget *delimiter_combo;
	GtkWidget *hours_check;
	GtkWidget *label_example1;
	GtkWidget *label_example2;

	/* ASR tab */
	GtkWidget     *asr_page;
	PtAsrSettings *asr_settings;
	GtkWidget     *asr_initial_box;
	GtkWidget     *asr_ready_box;
	GtkWidget     *add_asr_button;
	GtkWidget     *remove_asr_button;
	GtkWidget     *asr_view;
	GtkListStore  *asr_store;
	GtkWidget     *lm_chooser;
	GtkWidget     *dict_chooser;
	GtkWidget     *hmm_chooser;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtPreferencesDialog, pt_preferences_dialog, GTK_TYPE_DIALOG)

static void
spin_back_changed_cb (GtkSpinButton       *spin,
                      PtPreferencesDialog *dlg)
{
	gtk_label_set_label (
			GTK_LABEL (dlg->priv->label_back),
			/* Translators: This is part of the Preferences dialog
			   or the "Go to ..." dialog. There is a label like
			   "Jump back:", "Jump forward:", "Jump back on pause:"
			   or "Go to position:" before. */
			ngettext ("second", "seconds",
				  (int) gtk_spin_button_get_value_as_int (spin)));
}

static void
spin_forward_changed_cb (GtkSpinButton       *spin,
                         PtPreferencesDialog *dlg)
{
	gtk_label_set_label (
			GTK_LABEL (dlg->priv->label_forward),
			ngettext ("second", "seconds",
				  (int) gtk_spin_button_get_value_as_int (spin)));
}

static void
spin_pause_changed_cb (GtkSpinButton       *spin,
                       PtPreferencesDialog *dlg)
{
	gtk_label_set_label (
			GTK_LABEL (dlg->priv->label_pause),
			ngettext ("second", "seconds",
				  (int) gtk_spin_button_get_value_as_int (spin)));
}

static void
update_example_timestamps (PtPreferencesDialog *dlg)
{
	gchar *example1;
	gchar *example2;

	example1 = pt_player_get_timestamp_for_time (dlg->priv->player, EXAMPLE_TIME_SHORT, EXAMPLE_TIME_SHORT);
	example2 = pt_player_get_timestamp_for_time (dlg->priv->player, EXAMPLE_TIME_LONG, EXAMPLE_TIME_LONG);
	gtk_label_set_text (GTK_LABEL (dlg->priv->label_example1), example1);
	gtk_label_set_text (GTK_LABEL (dlg->priv->label_example2), example2);
	g_free (example1);
	g_free (example2);
}

static void
hours_check_toggled (GtkToggleButton     *check,
                     PtPreferencesDialog *dlg)
{
	g_settings_set_boolean (
			dlg->priv->editor, "timestamp-fixed",
			gtk_toggle_button_get_active (check));
	update_example_timestamps (dlg);
}

static void
delimiter_combo_changed (GtkComboBox         *widget,
                         PtPreferencesDialog *dlg)
{
	g_settings_set_string (
			dlg->priv->editor, "timestamp-delimiter",
			gtk_combo_box_get_active_id (widget));
	update_example_timestamps (dlg);
}

static void
separator_combo_changed (GtkComboBox         *widget,
                         PtPreferencesDialog *dlg)
{
	g_settings_set_string (
			dlg->priv->editor, "timestamp-fraction-sep",
			gtk_combo_box_get_active_id (widget));
	update_example_timestamps (dlg);
}

static void
precision_combo_changed (GtkComboBox         *widget,
                         PtPreferencesDialog *dlg)
{
	gint precision = gtk_combo_box_get_active (widget);
	g_settings_set_int (
			dlg->priv->editor, "timestamp-precision",
			precision);
	gtk_widget_set_sensitive (dlg->priv->separator_combo, (precision == 0) ? FALSE : TRUE);
	update_example_timestamps (dlg);
}

static void
asr_assistant_cancel_cb (GtkAssistant *assistant,
                         gpointer      user_data)
{
	gtk_widget_destroy (GTK_WIDGET (assistant));
}

static void
asr_assistant_close_cb (GtkAssistant        *assistant,
                        PtPreferencesDialog *dlg)
{
	GtkTreeSelection *sel;
	GtkTreeIter       iter;
	gchar            *name;
	gchar            *id;
	gboolean          empty;

	name = pt_asr_assistant_get_name (PT_ASR_ASSISTANT (assistant));
	id = pt_asr_assistant_save_config (
			PT_ASR_ASSISTANT (assistant), dlg->priv->asr_settings);
	gtk_widget_destroy (GTK_WIDGET (assistant));

	empty = !gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dlg->priv->asr_store), &iter);
	gtk_list_store_append (dlg->priv->asr_store, &iter);
	gtk_list_store_set (dlg->priv->asr_store, &iter, 0, FALSE, 1, id, 2, name, -1);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dlg->priv->asr_view));
	gtk_tree_selection_select_iter (sel, &iter);

	if (empty) {
		gtk_widget_hide (dlg->priv->asr_initial_box);
		gtk_widget_show (dlg->priv->asr_ready_box);
		/* Make it the default config in settings and in the view */
		g_settings_set_string (dlg->priv->editor, "asr-config", id);
		gtk_list_store_set (dlg->priv->asr_store, &iter, 0, TRUE, -1);
	}

	g_free (name);
	g_free (id);
}

static void
launch_asr_assistant (PtPreferencesDialog *dlg)
{
	GtkWindow *parent;
	GtkWidget *assistant;

	g_object_get (dlg, "transient-for", &parent, NULL);
	assistant = GTK_WIDGET (pt_asr_assistant_new (parent));
	g_signal_connect (assistant, "cancel", G_CALLBACK (asr_assistant_cancel_cb), NULL);
	g_signal_connect (assistant, "close", G_CALLBACK (asr_assistant_close_cb), dlg);
	gtk_widget_show (assistant);
	g_object_unref (parent);
}

static void
add_asr_button_clicked_cb (GtkToolButton       *button,
                           PtPreferencesDialog *dlg)
{
	launch_asr_assistant (dlg);
}

static void
initial_asr_button_clicked_cb (GtkButton           *button,
                               PtPreferencesDialog *dlg)
{
	launch_asr_assistant (dlg);
}

gboolean
confirm_delete (GtkWindow *parent,
                gchar     *name)
{
	g_assert_nonnull (name);

	GtkWidget       *dialog;
	gchar		*message;
	gchar		*secondary_message;
	gint		 response;

	message = _("Deleting configuration");
	secondary_message = g_strdup_printf (_("Do you really want to delete “%s”?"), name);
	dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_YES_NO,
                                   "%s", message);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary_message);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_free (secondary_message);

	return (response == GTK_RESPONSE_YES);
}

static void
remove_asr_button_clicked_cb (GtkToolButton       *button,
                              PtPreferencesDialog *dlg)
{
	GtkTreeSelection *sel;
	GtkTreeIter       iter;
	gboolean          active;
	gboolean          last;
	gchar            *name;
	gchar            *id;

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dlg->priv->asr_view));

	if (!gtk_tree_selection_get_selected (sel, NULL, &iter))
		return;

	gtk_tree_model_get (
			GTK_TREE_MODEL (dlg->priv->asr_store), &iter,
			0, &active, 1, &id, 2, &name, -1);
	if (confirm_delete (GTK_WINDOW (dlg), name)) {
		gtk_list_store_remove (dlg->priv->asr_store, &iter);
		pt_asr_settings_remove_config (dlg->priv->asr_settings, id);

		last = !gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dlg->priv->asr_store), &iter);
		if (last) {
			gtk_widget_hide (dlg->priv->asr_ready_box);
			gtk_widget_show (dlg->priv->asr_initial_box);
			g_settings_set_string (dlg->priv->editor, "asr-config", "none");
		} else {
			gtk_tree_selection_select_iter (sel, &iter);
			if (active) {
				g_free (id);
				gtk_tree_model_get (
						GTK_TREE_MODEL (dlg->priv->asr_store),
						&iter, 1, &id, -1);
				g_settings_set_string (dlg->priv->editor, "asr-config", id);
				gtk_list_store_set (dlg->priv->asr_store, &iter, 0, TRUE, -1);
			}
		}
	}

	g_free (name);
	g_free (id);
}

static void
clear_active (GtkListStore *store)
{
	GtkTreeIter iter;
	gboolean    valid;

	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);

	while (valid) {
		gtk_list_store_set (store, &iter, 0, FALSE, -1);
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
	}
}

static void
set_active (PtPreferencesDialog *dlg,
            GtkTreePath         *path)
{
	GtkListStore *store = dlg->priv->asr_store;
	GtkTreeIter   iter;
	gboolean      active;
	gchar        *new;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_tree_model_get (
			GTK_TREE_MODEL (store), &iter, 0, &active, 1, &new, -1);
	if (!active) {
		clear_active (store);
		gtk_list_store_set (store, &iter, 0, TRUE, -1);
		g_settings_set_string (dlg->priv->editor, "asr-config", new);
	}
	g_free (new);
}

static void
asr_default_toggled_cb (GtkCellRendererToggle *cell,
                        gchar                 *path_string,
                        gpointer               user_data)
{
	PtPreferencesDialog *dlg = PT_PREFERENCES_DIALOG (user_data);

	GtkTreePath *path;

	path = gtk_tree_path_new_from_string (path_string);
	set_active (dlg, path);
	gtk_tree_path_free (path);
}

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

static gchar*
get_uri_for_path (gchar *path)
{
	GFile *file;
	gchar *uri;

	file = g_file_new_for_path (path);
	uri = g_file_get_uri (file);
	g_object_unref (file);

	return uri;
}

static gchar*
get_selected_id (GtkTreeView *view)
{
	GtkTreeSelection *sel;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar            *id;

	sel = gtk_tree_view_get_selection (view);
	gtk_tree_selection_get_selected (sel, &model, &iter);
	gtk_tree_model_get (model, &iter, 1, &id, -1);

	return id;
}

static void
prefs_lm_chooser_file_set_cb (GtkFileChooserButton *button,
                              PtPreferencesDialog  *dlg)
{
	gchar *id;
	gchar *lm_uri, *lm;

	lm_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
	lm = get_path_for_uri (lm_uri);
	id = get_selected_id (GTK_TREE_VIEW (dlg->priv->asr_view));
	pt_asr_settings_set_string (dlg->priv->asr_settings, id, "lm", lm);

	g_free (id);
	g_free (lm);
	g_free (lm_uri);
}

static void
prefs_dict_chooser_file_set_cb (GtkFileChooserButton *button,
                                PtPreferencesDialog  *dlg)
{
	gchar *id;
	gchar *dict_uri, *dict;

	dict_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
	dict = get_path_for_uri (dict_uri);
	id = get_selected_id (GTK_TREE_VIEW (dlg->priv->asr_view));
	pt_asr_settings_set_string (dlg->priv->asr_settings, id, "dict", dict);

	g_free (id);
	g_free (dict);
	g_free (dict_uri);
}

static void
prefs_hmm_chooser_file_set_cb (GtkFileChooserButton *button,
                               PtPreferencesDialog  *dlg)
{
	gchar *id;
	gchar *hmm_uri, *hmm;

	hmm_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (button));
	hmm = get_path_for_uri (hmm_uri);
	id = get_selected_id (GTK_TREE_VIEW (dlg->priv->asr_view));
	pt_asr_settings_set_string (dlg->priv->asr_settings, id, "hmm", hmm);

	g_free (id);
	g_free (hmm);
	g_free (hmm_uri);
}

static void
asr_selection_changed_cb (GtkTreeSelection *sel,
                          gpointer          user_data)
{
	PtPreferencesDialog *dlg = PT_PREFERENCES_DIALOG (user_data);

	GtkTreeIter  iter;
	gchar       *id;
	gchar       *lm, *dict, *hmm;
	gchar       *lm_uri, *dict_uri, *hmm_uri;

	if (!gtk_tree_selection_get_selected (sel, NULL, &iter))
		return;

	gtk_tree_model_get (
			GTK_TREE_MODEL (dlg->priv->asr_store),
			&iter, 1, &id, -1);

	lm = pt_asr_settings_get_string (dlg->priv->asr_settings, id, "lm");
	dict = pt_asr_settings_get_string (dlg->priv->asr_settings, id, "dict");
	hmm = pt_asr_settings_get_string (dlg->priv->asr_settings, id, "hmm");
	lm_uri = get_uri_for_path (lm);
	dict_uri = get_uri_for_path (dict);
	hmm_uri = get_uri_for_path (hmm);

	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dlg->priv->lm_chooser), lm_uri);
	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dlg->priv->dict_chooser), dict_uri);
	gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dlg->priv->hmm_chooser), hmm_uri);

	g_free (id);
	g_free (lm);
	g_free (lm_uri);
	g_free (dict);
	g_free (dict_uri);
	g_free (hmm);
	g_free (hmm_uri);
}

static void
setup_non_wayland_env (PtPreferencesDialog *dlg)
{
	g_settings_bind (
		dlg->priv->editor, "remember-position",
		dlg->priv->pos_check, "active",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		dlg->priv->editor, "start-on-top",
		dlg->priv->top_check, "active",
		G_SETTINGS_BIND_DEFAULT);
}

static void
pt_preferences_dialog_init (PtPreferencesDialog *dlg)
{
	dlg->priv = pt_preferences_dialog_get_instance_private (dlg);
	dlg->priv->editor = g_settings_new (APP_ID);
	dlg->priv->player = pt_player_new ();
	pt_player_setup_player (dlg->priv->player, NULL); /* no error handling, already checked in main window */

	GApplication *app;
	app = g_application_get_default ();
	dlg->priv->asr_settings = g_object_ref (pt_app_get_asr_settings (PT_APP (app)));

	gtk_widget_init_template (GTK_WIDGET (dlg));
	g_settings_bind (
			dlg->priv->editor, "rewind-on-pause",
			dlg->priv->spin_pause, "value",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "jump-back",
			dlg->priv->spin_back, "value",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "jump-forward",
			dlg->priv->spin_forward, "value",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "repeat-all",
			dlg->priv->repeat_all_checkbox, "active",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "repeat-selection",
			dlg->priv->repeat_selection_checkbox, "active",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "remember-size",
			dlg->priv->size_check, "active",
			G_SETTINGS_BIND_DEFAULT);

#ifdef GDK_WINDOWING_WIN32
	setup_non_wayland_env (dlg);
#else
	GdkDisplay *display;
	display = gdk_display_get_default ();
#endif
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (display))
		setup_non_wayland_env (dlg);
#endif
#ifdef GDK_WINDOWING_WAYLAND
	if (GDK_IS_WAYLAND_DISPLAY (display)) {
		gtk_widget_hide (dlg->priv->pos_check);
		gtk_widget_hide (dlg->priv->top_check);
	}
#endif

	g_settings_bind (
			dlg->priv->editor, "show-ruler",
			dlg->priv->ruler_check, "active",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "fixed-cursor",
			dlg->priv->fixed_cursor_radio, "active",
			G_SETTINGS_BIND_DEFAULT);

	GtkAdjustment *pps_adj;
	pps_adj = gtk_range_get_adjustment (GTK_RANGE (dlg->priv->pps_scale));
	g_settings_bind (
			dlg->priv->editor, "pps",
			pps_adj, "value",
			G_SETTINGS_BIND_DEFAULT);
	GtkScale *scale = GTK_SCALE (dlg->priv->pps_scale);
	gtk_scale_add_mark (scale, 25, GTK_POS_TOP, NULL);
	gtk_scale_add_mark (scale, 62.5, GTK_POS_TOP, NULL);
	gtk_scale_add_mark (scale, 100, GTK_POS_TOP, NULL);
	gtk_scale_add_mark (scale, 150, GTK_POS_TOP, NULL);
	gtk_scale_add_mark (scale, 200, GTK_POS_TOP, NULL);

	/* make sure labels are set and translated */
	spin_back_changed_cb (GTK_SPIN_BUTTON (dlg->priv->spin_back), dlg);
	spin_forward_changed_cb (GTK_SPIN_BUTTON (dlg->priv->spin_forward), dlg);
	spin_pause_changed_cb (GTK_SPIN_BUTTON (dlg->priv->spin_pause), dlg);

	/* Bind settings to our player instance for example timestamps */
	g_settings_bind (
			dlg->priv->editor, "timestamp-precision",
			dlg->priv->player, "timestamp-precision",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			dlg->priv->editor, "timestamp-fixed",
			dlg->priv->player, "timestamp-fixed",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			dlg->priv->editor, "timestamp-delimiter",
			dlg->priv->player, "timestamp-delimiter",
			G_SETTINGS_BIND_GET);

	g_settings_bind (
			dlg->priv->editor, "timestamp-fraction-sep",
			dlg->priv->player, "timestamp-fraction-sep",
			G_SETTINGS_BIND_GET);

	/* setup timestamp fields according to settings */
	gchar *delimiter = g_settings_get_string (dlg->priv->editor, "timestamp-delimiter");
	if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (dlg->priv->delimiter_combo), delimiter)) {
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (dlg->priv->delimiter_combo), "#");
	}
	gchar *sep = g_settings_get_string (dlg->priv->editor, "timestamp-fraction-sep");
	if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (dlg->priv->separator_combo), sep)) {
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (dlg->priv->separator_combo), ".");
	}
	gint precision = g_settings_get_int (dlg->priv->editor, "timestamp-precision");
	if (precision < 0 || precision > 2)
		precision = 1;
	gtk_combo_box_set_active (GTK_COMBO_BOX (dlg->priv->precision_combo), precision);
	gboolean fixed = g_settings_get_boolean (dlg->priv->editor, "timestamp-fixed");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dlg->priv->hours_check), fixed);
	g_free (delimiter);
	g_free (sep);

	/* setup ASR page */
	if (!pt_app_get_asr (PT_APP (app))) {
		gint num;
		num = gtk_notebook_page_num (GTK_NOTEBOOK (dlg->priv->notebook), dlg->priv->asr_page);
		g_assert (num != -1);
		gtk_notebook_remove_page (GTK_NOTEBOOK (dlg->priv->notebook), num);
		return;
	}

	GtkTreeSelection *sel;
	GtkTreeIter       row;
	GtkTreeIter       active_row;
	gchar           **configs = NULL;
	gchar            *active_id;
	gboolean          active;
	gint              i = 0;

	dlg->priv->asr_store = gtk_list_store_new (3,
					G_TYPE_BOOLEAN,		/* active */
					G_TYPE_STRING,		/* ID     */
					G_TYPE_STRING);		/* name   */

	configs = pt_asr_settings_get_configs (dlg->priv->asr_settings);
	gtk_tree_view_set_model (GTK_TREE_VIEW (dlg->priv->asr_view), GTK_TREE_MODEL (dlg->priv->asr_store));
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dlg->priv->asr_view));
	g_signal_connect (sel, "changed", G_CALLBACK (asr_selection_changed_cb), dlg);

	if (!configs[0]) {
		gtk_widget_hide (dlg->priv->asr_ready_box);
		g_strfreev (configs);
		return;
	}

	gtk_widget_hide (dlg->priv->asr_initial_box);
	active_id = g_settings_get_string (dlg->priv->editor, "asr-config");
	while (configs[i]) {
		active = (g_strcmp0 (active_id, configs[i]) == 0);
		gtk_list_store_append (dlg->priv->asr_store, &row);
		gtk_list_store_set (dlg->priv->asr_store, &row,
				0, active,
				1, configs[i],
				2, pt_asr_settings_get_string (dlg->priv->asr_settings, configs[i], "name"),
				-1);
		if (i == 0)
			active_row = row;
		if (active)
			active_row = row;
		i++;
	}
	gtk_tree_selection_select_iter (sel, &active_row);

	g_free (active_id);
	g_strfreev (configs);
}

static void
pt_preferences_dialog_dispose (GObject *object)
{
	PtPreferencesDialog *dlg = PT_PREFERENCES_DIALOG (object);

	g_clear_object (&dlg->priv->editor);
	g_clear_object (&dlg->priv->player);
	g_clear_object (&dlg->priv->asr_settings);

	G_OBJECT_CLASS (pt_preferences_dialog_parent_class)->dispose (object);
}

static void
pt_preferences_dialog_class_init (PtPreferencesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = pt_preferences_dialog_dispose;

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/preferences.ui");
	gtk_widget_class_bind_template_callback(widget_class, add_asr_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, asr_default_toggled_cb);
	gtk_widget_class_bind_template_callback(widget_class, delimiter_combo_changed);
	gtk_widget_class_bind_template_callback(widget_class, hours_check_toggled);
	gtk_widget_class_bind_template_callback(widget_class, initial_asr_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, precision_combo_changed);
	gtk_widget_class_bind_template_callback(widget_class, prefs_hmm_chooser_file_set_cb);
	gtk_widget_class_bind_template_callback(widget_class, prefs_dict_chooser_file_set_cb);
	gtk_widget_class_bind_template_callback(widget_class, prefs_lm_chooser_file_set_cb);
	gtk_widget_class_bind_template_callback(widget_class, remove_asr_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, separator_combo_changed);
	gtk_widget_class_bind_template_callback(widget_class, spin_back_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, spin_forward_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, spin_pause_changed_cb);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, notebook);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, spin_pause);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, spin_back);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, spin_forward);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, repeat_all_checkbox);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, repeat_selection_checkbox);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, pps_scale);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, size_check);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, pos_check);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, top_check);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_pause);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_back);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_forward);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, ruler_check);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, fixed_cursor_radio);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, precision_combo);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, separator_combo);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, delimiter_combo);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, hours_check);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_example1);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_example2);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, asr_page);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, add_asr_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, remove_asr_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, asr_view);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, lm_chooser);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, dict_chooser);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, hmm_chooser);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, asr_initial_box);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, asr_ready_box);
}

void
pt_show_preferences_dialog (GtkWindow *parent)
{
	if (preferences_dialog == NULL)	{
	
		preferences_dialog = GTK_WIDGET (g_object_new (PT_TYPE_PREFERENCES_DIALOG,
				"use-header-bar", TRUE,
				NULL));
		g_signal_connect (preferences_dialog,
				  "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &preferences_dialog);
	}
	
	if (parent != gtk_window_get_transient_for (GTK_WINDOW (preferences_dialog))) {
		gtk_window_set_transient_for (GTK_WINDOW (preferences_dialog), GTK_WINDOW (parent));
		gtk_window_set_modal (GTK_WINDOW (preferences_dialog), FALSE);
		gtk_window_set_destroy_with_parent (GTK_WINDOW (preferences_dialog), TRUE);
	}

	gtk_window_present (GTK_WINDOW (preferences_dialog));
}
