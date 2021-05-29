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
#include <parlatype.h>
#include "pt-asr-dialog.h"
#include "pt-preferences.h"
#include "pt-config-row.h"

struct _PtConfigRowPrivate
{
	PtConfig  *config;
	GtkWidget *name_label;
	GtkWidget *lang_label;
	GtkWidget *details_button;
	GtkWidget *status_image;
	gboolean   active;
	gboolean   supported;
	gboolean   installed;
};

enum
{
	PROP_0,
	PROP_CONFIG,
	PROP_ACTIVE,
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


gboolean
pt_config_row_is_installed (PtConfigRow *row)
{
	return row->priv->installed;
}

gboolean
pt_config_row_get_active (PtConfigRow *row)
{
	return row->priv->active;
}

static void
set_status_image (PtConfigRow *row)
{
	GtkImage *status = GTK_IMAGE (row->priv->status_image);
	gchar    *icon = NULL;

	if (!row->priv->supported)
		icon = "action-unavailable-symbolic";
	else if (!row->priv->installed)
		icon = "folder-download-symbolic";
	else if (row->priv->active)
		icon = "object-select-symbolic";

	if (icon)
		gtk_image_set_from_icon_name (status, icon,
		                              GTK_ICON_SIZE_BUTTON);
	else
		gtk_image_clear (status);
}

void
pt_config_row_set_active (PtConfigRow *row,
                          gboolean     active)
{
	PtConfigRowPrivate *priv = row->priv;

	if (priv->active == active ||
	    !priv->supported       ||
	    !priv->installed)
		return;

	priv->active = active;
	g_object_notify_by_pspec (G_OBJECT (row),
	                          obj_properties[PROP_ACTIVE]);
	set_status_image (row);
}

gboolean
pt_config_row_get_supported (PtConfigRow *row)
{
	return row->priv->supported;
}

void
pt_config_row_set_supported (PtConfigRow *row,
                             gboolean     supported)
{
	row->priv->supported = supported;
	set_status_image (row);
}

static void
config_is_installed_cb (PtConfig   *config,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
	PtConfigRow *row = PT_CONFIG_ROW (user_data);
	row->priv->installed = pt_config_is_installed (config);
	set_status_image (row);
}

static void
pt_config_row_constructed (GObject *object)
{
	PtConfigRow *row = PT_CONFIG_ROW (object);
	PtConfigRowPrivate *priv = row->priv;

	g_object_bind_property (
			priv->config, "name",
			priv->name_label, "label",
			G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	gtk_label_set_text (GTK_LABEL (priv->lang_label),
			    pt_config_get_lang_name (priv->config));

	priv->installed = pt_config_is_installed (priv->config);
	g_signal_connect (
			priv->config, "notify::is-installed",
			G_CALLBACK (config_is_installed_cb), row);
}

static void
pt_config_row_init (PtConfigRow *row)
{
	row->priv = pt_config_row_get_instance_private (row);

	row->priv->active    = FALSE;
	row->priv->supported = FALSE;
	row->priv->installed = FALSE;

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
	case PROP_ACTIVE:
		row->priv->active = g_value_get_boolean (value);
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
	case PROP_ACTIVE:
		g_value_set_boolean (value, row->priv->active);
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
	gtk_widget_class_bind_template_child_private (widget_class, PtConfigRow, status_image);

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

	/**
	* PtConfigRow:active:
	*
	* Whether the configuration is active.
	*/
	obj_properties[PROP_ACTIVE] =
	g_param_spec_boolean (
			"active",
			"Active",
			"The configuration is active",
			FALSE,
			G_PARAM_READWRITE);

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
