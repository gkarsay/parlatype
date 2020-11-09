/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
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
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <pt-config.h>
#include "pt-config-row.h"

struct _PtConfigRowPrivate
{
	PtConfig  *config;
	GtkWidget *name_label;
	GtkWidget *lang_label;
	GtkWidget *details_button;
	GtkWidget *install_button;
	GtkWidget *activate_button;
	GtkWidget *active_icon;
};

enum
{
	PROP_0,
	PROP_CONFIG,
	N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };


G_DEFINE_TYPE_WITH_PRIVATE (PtConfigRow, pt_config_row, GTK_TYPE_LIST_BOX_ROW)


/**
 * SECTION: pt-config-row
 * @short_description:
 * @stability: Unstable
 * @include: parlatype/pt-config-row.h
 *
 * TODO
 */


static void
details_clicked (GtkButton *button,
                PtConfigRow *row)
{
	g_print ("Details not implemented\n");
}

static void
install_dialog_response_cb (GtkDialog *dialog,
                            gint       response_id,
                            gpointer   user_data)
{
	PtConfigRow *row = PT_CONFIG_ROW (user_data);
	gchar       *folder = NULL;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		folder = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	}

	if (folder) {
		pt_config_set_base_folder (row->priv->config, folder);
		g_free (folder);
	}

	g_object_unref (dialog);
}


static void
install_clicked (GtkButton *button,
                 PtConfigRow *row)
{
	GtkFileChooserNative *dialog;
	GtkWindow     *win;
	const char    *home_path;
	GFile	      *dir;

	win = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button)));
	dialog = gtk_file_chooser_native_new (
			_("Open Installation Folder"),
			win,
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
			_("_Open"),
			_("_Cancel"));

	home_path = g_get_home_dir ();
	dir = g_file_new_for_path (home_path);

	gtk_file_chooser_set_current_folder_file (
		GTK_FILE_CHOOSER (dialog), dir, NULL);

	g_signal_connect (dialog, "response", G_CALLBACK (install_dialog_response_cb), row);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));

	g_object_unref (dir);
}

static void
activate_clicked (GtkButton *button,
                  PtConfigRow *row)
{
	g_print ("Activate button not implemented\n");
}

gboolean
pt_config_row_get_active (PtConfigRow *row)
{
	return gtk_widget_get_visible (row->priv->active_icon);
}

void
pt_config_row_set_active (PtConfigRow *row,
                          gboolean     active)
{
	gtk_widget_set_visible (row->priv->active_icon, active);
}

static void
pt_config_row_constructed (GObject *object)
{
	PtConfigRow *row = PT_CONFIG_ROW (object);
	PtConfigRowPrivate *priv = row->priv;

	gtk_label_set_text (GTK_LABEL (priv->name_label),
			    pt_config_get_name (priv->config));
	gtk_label_set_text (GTK_LABEL (priv->lang_label),
			    pt_config_get_lang_name (priv->config));
	g_object_bind_property (priv->config, "is-installed",
				priv->activate_button, "sensitive",
				G_BINDING_SYNC_CREATE);
}

static void
pt_config_row_init (PtConfigRow *row)
{
	row->priv = pt_config_row_get_instance_private (row);

	gtk_widget_init_template (GTK_WIDGET (row));
}

static void
pt_config_row_dispose (GObject *object)
{
	G_OBJECT_CLASS (pt_config_row_parent_class)->dispose (object);
}

static void
pt_config_row_finalize (GObject *object)
{
	PtConfigRow *row = PT_CONFIG_ROW (object);

	g_object_unref (row->priv->config);

	G_OBJECT_CLASS (pt_config_row_parent_class)->finalize (object);
}

static void
pt_config_row_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
	PtConfigRow *row = PT_CONFIG_ROW (object);

	switch (property_id) {
	case PROP_CONFIG:
		row->priv->config = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_config_row_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
	PtConfigRow *row = PT_CONFIG_ROW (object);

	switch (property_id) {
	case PROP_CONFIG:
		g_value_set_object (value, row->priv->config);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_config_row_class_init (PtConfigRowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = pt_config_row_set_property;
	object_class->get_property = pt_config_row_get_property;
	object_class->constructed  = pt_config_row_constructed;
	object_class->dispose = pt_config_row_dispose;
	object_class->finalize = pt_config_row_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/config-row.ui");
	gtk_widget_class_bind_template_child_private (widget_class, PtConfigRow, name_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtConfigRow, lang_label);
	gtk_widget_class_bind_template_child_private (widget_class, PtConfigRow, details_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtConfigRow, install_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtConfigRow, activate_button);
	gtk_widget_class_bind_template_child_private (widget_class, PtConfigRow, active_icon);
	gtk_widget_class_bind_template_callback (widget_class, details_clicked);
	gtk_widget_class_bind_template_callback (widget_class, install_clicked);
	gtk_widget_class_bind_template_callback (widget_class, activate_clicked);

	/**
	* PtConfigRow:config:
	*
	* The configuration object.
	*/
	obj_properties[PROP_CONFIG] =
	g_param_spec_object (
			"config",
			"Config",
			"The configuration",
			PT_TYPE_CONFIG,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);
}

/**
 * pt_config_row_new:
 * @row_file: path to the file with the settings
 *
 * Returns a new PtConfigRow instance for the given file. If the file
 * doesn’t exist, it will be created.
 *
 * After use g_object_unref() it.
 *
 * Return value: (transfer full): a new #PtConfigRow
 */
PtConfigRow *
pt_config_row_new (PtConfig *config)
{
	g_return_val_if_fail (config != NULL, NULL);
	g_return_val_if_fail (PT_IS_CONFIG (config), NULL);

	return g_object_new (PT_TYPE_CONFIG_ROW,
			"config", config,
			NULL);
}
