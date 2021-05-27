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


#include <gio/gio.h>
#include "mock-plugin.h"

struct _MockPluginPrivate
{
	gchar    *prop_file;
	gchar    *prop_string;
	gint      prop_int;
	gfloat    prop_float;
	gdouble   prop_double;
	gboolean  prop_bool;
	gboolean  prop_not_writable;
};

enum
{
	PROP_0,
	PROP_FILE,
	PROP_STRING,
	PROP_INT,
	PROP_FLOAT,
	PROP_DOUBLE,
	PROP_BOOL,
	PROP_NOT_WRITABLE,
	N_PROPERTIES
};


static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (MockPlugin, mock_plugin, GST_TYPE_ELEMENT)


static void
mock_plugin_init (MockPlugin *self)
{
	self->priv = mock_plugin_get_instance_private (self);
}

static void
mock_plugin_finalize (GObject *object)
{
	MockPlugin *self = MOCK_PLUGIN (object);

	g_free (self->priv->prop_file);
	g_free (self->priv->prop_string);

	G_OBJECT_CLASS (mock_plugin_parent_class)->finalize (object);
}

static void
mock_plugin_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
	MockPlugin *self = MOCK_PLUGIN (object);

	switch (property_id) {
	case PROP_FILE:
		g_free (self->priv->prop_file);
		self->priv->prop_file = g_value_dup_string (value);
		break;
	case PROP_STRING:
		g_free (self->priv->prop_string);
		self->priv->prop_string = g_value_dup_string (value);
		break;
	case PROP_INT:
		self->priv->prop_int    = g_value_get_int (value);
		break;
	case PROP_FLOAT:
		self->priv->prop_float  = g_value_get_float (value);
		break;
	case PROP_DOUBLE:
		self->priv->prop_double = g_value_get_double (value);
		break;
	case PROP_BOOL:
		self->priv->prop_bool   = g_value_get_boolean (value);
		break;
	case PROP_NOT_WRITABLE:
		self->priv->prop_not_writable = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
mock_plugin_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
	MockPlugin *self = MOCK_PLUGIN (object);

	switch (property_id) {
	case PROP_FILE:
		g_value_set_string (value, self->priv->prop_file);
		break;
	case PROP_STRING:
		g_value_set_string (value, self->priv->prop_string);
		break;
	case PROP_INT:
		g_value_set_int (value, self->priv->prop_int);
		break;
	case PROP_FLOAT:
		g_value_set_float (value, self->priv->prop_float);
		break;
	case PROP_DOUBLE:
		g_value_set_double (value, self->priv->prop_double);
		break;
	case PROP_BOOL:
		g_value_set_boolean (value, self->priv->prop_bool);
		break;
	case PROP_NOT_WRITABLE:
		g_value_set_int (value, self->priv->prop_not_writable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
mock_plugin_class_init (MockPluginClass *klass)
{
	GObjectClass    *object_class  = (GObjectClass *) klass;
	GstElementClass *element_class = (GstElementClass *) klass;;

	object_class->set_property = mock_plugin_set_property;
	object_class->get_property = mock_plugin_get_property;
	object_class->finalize     = mock_plugin_finalize;

	obj_properties[PROP_FILE] =
	g_param_spec_string (
			"file", "file", "file",
			NULL,
			G_PARAM_READWRITE);

	obj_properties[PROP_STRING] =
	g_param_spec_string (
			"string", "string", "string",
			NULL,
			G_PARAM_READWRITE);

	obj_properties[PROP_INT] =
	g_param_spec_int (
			"int", "int", "int",
			-100, 100, 0,
			G_PARAM_READWRITE);

	obj_properties[PROP_FLOAT] =
	g_param_spec_float (
			"float", "float", "float",
			-G_MAXFLOAT, G_MAXFLOAT, 0,
			G_PARAM_READWRITE);

	obj_properties[PROP_DOUBLE] =
	g_param_spec_double (
			"double", "double", "double",
			-G_MAXDOUBLE, G_MAXDOUBLE, 0,
			G_PARAM_READWRITE);

	obj_properties[PROP_BOOL] =
	g_param_spec_boolean (
			"bool", "bool", "bool",
			FALSE,
			G_PARAM_READWRITE);

	obj_properties[PROP_NOT_WRITABLE] =
	g_param_spec_int (
			"not-writable", "not-writable", "not-writable",
			G_MININT, G_MAXINT, 0,
			G_PARAM_READABLE);

	g_object_class_install_properties (
			G_OBJECT_CLASS (klass),
			N_PROPERTIES,
			obj_properties);

	gst_element_class_set_static_metadata (
		element_class,
		"Parlatype Mock Plugin",
		"Filter/Audio",
		"Just for testing",
		"Gabor Karsay <gabor.karsay@gmx.at>");
}

static gboolean
plugin_init (GstPlugin *plugin)
{
	if (!gst_element_register(plugin,
	                          "ptmockplugin",
	                          GST_RANK_NONE,
	                          MOCK_TYPE_PLUGIN))
		return FALSE;
	return TRUE;
}

gboolean
mock_plugin_register (void)
{
	return gst_plugin_register_static (
			GST_VERSION_MAJOR,
			GST_VERSION_MINOR,
			"ptmockplugin",
			"Parlatype Mock Plugin for testing",
			plugin_init,
			"1.0",
			"LGPL",
			"libparlatype",
			"Parlatype",
			"https://www.parlatype.org/");
}

MockPlugin *
mock_plugin_new (void)
{
	return g_object_new (MOCK_TYPE_PLUGIN, NULL);
}
