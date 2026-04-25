/* Copyright 2021 Gabor Karsay <gabor.karsay@gmx.at>
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

#include "mock-plugin.h"

#include "parlatype.h"

#include <gio/gio.h>

struct _PtMockPlugin
{
  GObject  parent_instance;
  gchar   *prop_file;
  gchar   *prop_string;
  gint     prop_int;
  gfloat   prop_float;
  gdouble  prop_double;
  gboolean prop_bool;
  gboolean prop_not_writable;
};

enum
{
  PROP_0,
  PROP_BUSY,
  PROP_FILE,
  PROP_STRING,
  PROP_INT,
  PROP_FLOAT,
  PROP_DOUBLE,
  PROP_BOOL,
  PROP_NOT_WRITABLE,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];
static void        pt_speech_recognizer_iface_init (PtSpeechRecognizerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (PtMockPlugin, pt_mock_plugin, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (PT_TYPE_SPEECH_RECOGNIZER, pt_speech_recognizer_iface_init))

static void
pt_mock_plugin_finalize (GObject *object)
{
  PtMockPlugin *self = PT_MOCK_PLUGIN (object);

  g_free (self->prop_file);
  g_free (self->prop_string);

  G_OBJECT_CLASS (pt_mock_plugin_parent_class)->finalize (object);
}

static void
pt_mock_plugin_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PtMockPlugin *self = PT_MOCK_PLUGIN (object);

  switch (property_id)
    {
    case PROP_FILE:
      g_free (self->prop_file);
      self->prop_file = g_value_dup_string (value);
      break;
    case PROP_STRING:
      g_free (self->prop_string);
      self->prop_string = g_value_dup_string (value);
      break;
    case PROP_INT:
      self->prop_int = g_value_get_int (value);
      break;
    case PROP_FLOAT:
      self->prop_float = g_value_get_float (value);
      break;
    case PROP_DOUBLE:
      self->prop_double = g_value_get_double (value);
      break;
    case PROP_BOOL:
      self->prop_bool = g_value_get_boolean (value);
      break;
    case PROP_NOT_WRITABLE:
      self->prop_not_writable = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_mock_plugin_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PtMockPlugin *self = PT_MOCK_PLUGIN (object);

  switch (property_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, FALSE);
      break;
    case PROP_FILE:
      g_value_set_string (value, self->prop_file);
      break;
    case PROP_STRING:
      g_value_set_string (value, self->prop_string);
      break;
    case PROP_INT:
      g_value_set_int (value, self->prop_int);
      break;
    case PROP_FLOAT:
      g_value_set_float (value, self->prop_float);
      break;
    case PROP_DOUBLE:
      g_value_set_double (value, self->prop_double);
      break;
    case PROP_BOOL:
      g_value_set_boolean (value, self->prop_bool);
      break;
    case PROP_NOT_WRITABLE:
      g_value_set_int (value, self->prop_not_writable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_speech_recognizer_iface_init (PtSpeechRecognizerInterface *iface)
{
}

static void
pt_mock_plugin_init (PtMockPlugin *self)
{
}

static void
pt_mock_plugin_class_init (PtMockPluginClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->set_property = pt_mock_plugin_set_property;
  object_class->get_property = pt_mock_plugin_get_property;
  object_class->finalize = pt_mock_plugin_finalize;

  obj_properties[PROP_BUSY] =
      g_param_spec_boolean (
          "busy", NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_FILE] =
      g_param_spec_string (
          "file", NULL, NULL,
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_STRING] =
      g_param_spec_string (
          "string", NULL, NULL,
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_INT] =
      g_param_spec_int (
          "int", NULL, NULL,
          -100, 100, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_FLOAT] =
      g_param_spec_float (
          "float", NULL, NULL,
          -G_MAXFLOAT, G_MAXFLOAT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_DOUBLE] =
      g_param_spec_double (
          "double", NULL, NULL,
          -G_MAXDOUBLE, G_MAXDOUBLE, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_BOOL] =
      g_param_spec_boolean (
          "bool", NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_NOT_WRITABLE] =
      g_param_spec_int (
          "not-writable", NULL, NULL,
          G_MININT, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      G_OBJECT_CLASS (klass),
      N_PROPERTIES,
      obj_properties);
}

PtMockPlugin *
pt_mock_plugin_new (void)
{
  return g_object_new (PT_TYPE_MOCK_PLUGIN, NULL);
}
