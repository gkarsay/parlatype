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
#include <parlatype.h>
#include "pt-app.h"
#include "pt-prefs-asr.h"
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
	GtkWidget *asr_page;
	GtkWidget *asr_label;

	dlg->priv = pt_preferences_dialog_get_instance_private (dlg);
	dlg->priv->editor = g_settings_new (APP_ID);
	dlg->priv->player = pt_player_new ();
	pt_player_setup_player (dlg->priv->player, TRUE);

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

	asr_page  = pt_prefs_asr_new (dlg->priv->editor, dlg->priv->player);
	asr_label = gtk_label_new (_("Speech recognition"));
	gtk_notebook_insert_page (GTK_NOTEBOOK (dlg->priv->notebook),
	                          asr_page, asr_label, 3);
}

static void
pt_preferences_dialog_dispose (GObject *object)
{
	PtPreferencesDialog *dlg = PT_PREFERENCES_DIALOG (object);

	g_clear_object (&dlg->priv->editor);
	g_clear_object (&dlg->priv->player);

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
	gtk_widget_class_bind_template_callback(widget_class, delimiter_combo_changed);
	gtk_widget_class_bind_template_callback(widget_class, hours_check_toggled);
	gtk_widget_class_bind_template_callback(widget_class, precision_combo_changed);
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
