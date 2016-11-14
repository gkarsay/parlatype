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
#include "pt-preferences.h"


static GtkWidget *preferences_dialog = NULL;

struct _PtPreferencesDialogPrivate
{
	GSettings *editor;

	GtkWidget *spin_pause;
	GtkWidget *spin_back;
	GtkWidget *spin_forward;
	GtkWidget *label_pause;
	GtkWidget *label_back;
	GtkWidget *label_forward;
	GtkWidget *check_pos;
	GtkWidget *check_top;
	GtkWidget *ruler_switch;
	GtkWidget *fixed_cursor_switch;
};

G_DEFINE_TYPE_WITH_PRIVATE (PtPreferencesDialog, pt_preferences_dialog, GTK_TYPE_DIALOG)

void
spin_back_changed_cb (GtkSpinButton	   *spin,
		      PtPreferencesDialog  *dlg)
{
	gtk_label_set_label (
			GTK_LABEL (dlg->priv->label_back),
			ngettext ("second", "seconds",
				  (int) gtk_spin_button_get_value_as_int (spin)));
}

void
spin_forward_changed_cb (GtkSpinButton	      *spin,
			 PtPreferencesDialog  *dlg)
{
	gtk_label_set_label (
			GTK_LABEL (dlg->priv->label_forward),
			ngettext ("second", "seconds",
				  (int) gtk_spin_button_get_value_as_int (spin)));
}

void
spin_pause_changed_cb (GtkSpinButton	    *spin,
		       PtPreferencesDialog  *dlg)
{
	gtk_label_set_label (
			GTK_LABEL (dlg->priv->label_pause),
			ngettext ("second", "seconds",
				  (int) gtk_spin_button_get_value_as_int (spin)));
}

static void
pt_preferences_dialog_init (PtPreferencesDialog *dlg)
{
	dlg->priv = pt_preferences_dialog_get_instance_private (dlg);
	dlg->priv->editor = g_settings_new ("org.gnome.parlatype");

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
			dlg->priv->editor, "remember-position",
			dlg->priv->check_pos, "active",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "start-on-top",
			dlg->priv->check_top, "active",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "show-ruler",
			dlg->priv->ruler_switch, "active",
			G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
			dlg->priv->editor, "fixed-cursor",
			dlg->priv->fixed_cursor_switch, "active",
			G_SETTINGS_BIND_DEFAULT);

#if GTK_CHECK_VERSION(3,12,0)
#else
	gtk_dialog_add_button (GTK_DIALOG (dlg), _("_Close"), -6);
	g_signal_connect_swapped (dlg, "response", G_CALLBACK (gtk_widget_destroy), dlg);
#endif
}

static void
pt_preferences_dialog_dispose (GObject *object)
{
	PtPreferencesDialog *dlg = PT_PREFERENCES_DIALOG (object);

	g_clear_object (&dlg->priv->editor);

	G_OBJECT_CLASS (pt_preferences_dialog_parent_class)->dispose (object);
}

static void
pt_preferences_dialog_class_init (PtPreferencesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = pt_preferences_dialog_dispose;

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/parlatype/preferences.ui");
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, spin_pause);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, spin_back);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, spin_forward);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, check_pos);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, check_top);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_pause);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_back);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, label_forward);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, ruler_switch);
	gtk_widget_class_bind_template_child_private (widget_class, PtPreferencesDialog, fixed_cursor_switch);
}

void
pt_show_preferences_dialog (GtkWindow *parent)
{
	if (preferences_dialog == NULL)	{
	
		preferences_dialog = GTK_WIDGET (g_object_new (PT_TYPE_PREFERENCES_DIALOG,
#if GTK_CHECK_VERSION(3,12,0)
				"use-header-bar", TRUE,
#endif
				NULL));
		g_signal_connect (preferences_dialog,
				  "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &preferences_dialog);
	}
	
	if (parent != gtk_window_get_transient_for (GTK_WINDOW (preferences_dialog))) {
		gtk_window_set_transient_for (GTK_WINDOW (preferences_dialog), GTK_WINDOW (parent));
		gtk_window_set_modal (GTK_WINDOW (preferences_dialog), TRUE);
		gtk_window_set_destroy_with_parent (GTK_WINDOW (preferences_dialog), TRUE);
	}

	gtk_window_present (GTK_WINDOW (preferences_dialog));
}

