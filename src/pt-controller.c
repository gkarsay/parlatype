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
#include "pt-window.h"
#include "pt-window-private.h"
#include "pt-controller.h"

typedef struct _PtControllerPrivate PtControllerPrivate;
struct _PtControllerPrivate
{
  PtWindow *win;
};

enum
{
  PROP_WIN = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (PtController, pt_controller, G_TYPE_OBJECT)

PtWindow *
pt_controller_get_window (PtController *self)
{
  PtControllerPrivate *priv = pt_controller_get_instance_private (self);
  return priv->win;
}

PtPlayer *
pt_controller_get_player (PtController *self)
{
  PtControllerPrivate *priv = pt_controller_get_instance_private (self);
  return _pt_window_get_player (priv->win);
}

static void
pt_controller_init (PtController *self)
{
}

static void
pt_controller_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  PtController *self = PT_CONTROLLER (object);
  PtControllerPrivate *priv = pt_controller_get_instance_private (self);

  switch (property_id)
    {
    case PROP_WIN:
      priv->win = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
pt_controller_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  PtController *self = PT_CONTROLLER (object);
  PtControllerPrivate *priv = pt_controller_get_instance_private (self);

  switch (property_id)
    {
    case PROP_WIN:
      g_value_set_object (value, priv->win);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}
static void
pt_controller_class_init (PtControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = pt_controller_set_property;
  object_class->get_property = pt_controller_get_property;

  obj_properties[PROP_WIN] =
      g_param_spec_object ("win", NULL, NULL,
                           PT_WINDOW_TYPE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (
      object_class,
      N_PROPERTIES,
      obj_properties);
}

PtController *
pt_controller_new (PtWindow *win)
{
  return g_object_new (PT_CONTROLLER_TYPE, "win", win, NULL);
}
