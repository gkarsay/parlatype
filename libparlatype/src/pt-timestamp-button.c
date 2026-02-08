/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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

#include "pt-timestamp-button.h"

#include "pt-timestamp.h"

struct _PtTimestampButton
{
  GtkButton parent_instance;
  int       timestamp;
};

enum
{
  PROP_0,
  PROP_TIMESTAMP,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtTimestampButton, pt_timestamp_button, GTK_TYPE_BUTTON)

static GParamSpec *properties[N_PROPS];

int
pt_timestamp_button_get_timestamp (PtTimestampButton *self)
{
  g_return_val_if_fail (PT_IS_TIMESTAMP_BUTTON (self), 0);

  return self->timestamp;
}

void
pt_timestamp_button_set_timestamp (PtTimestampButton *self,
                                   int                timestamp)
{
  g_return_if_fail (PT_IS_TIMESTAMP_BUTTON (self));
  g_return_if_fail (timestamp >= 0);

  char *label;

  if (timestamp == self->timestamp)
    return;

  self->timestamp = timestamp;
  label = pt_timestamp_to_string (self->timestamp, -1, PT_PRECISION_SECOND_10TH);
  gtk_button_set_label (GTK_BUTTON (self), label);
  g_free (label);
}

static void
pt_timestamp_button_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  PtTimestampButton *self = PT_TIMESTAMP_BUTTON (object);

  switch (prop_id)
    {
    case PROP_TIMESTAMP:
      pt_timestamp_button_set_timestamp (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_timestamp_button_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  PtTimestampButton *self = PT_TIMESTAMP_BUTTON (object);

  switch (prop_id)
    {
    case PROP_TIMESTAMP:
      g_value_set_int (value, self->timestamp);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_timestamp_button_init (PtTimestampButton *self)
{
}

static void
pt_timestamp_button_class_init (PtTimestampButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = pt_timestamp_button_get_property;
  object_class->set_property = pt_timestamp_button_set_property;

  properties[PROP_TIMESTAMP] =
      g_param_spec_int ("timestamp", NULL, NULL,
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

GtkWidget *
pt_timestamp_button_new (int timestamp)
{
  g_return_val_if_fail (timestamp >= 0, NULL);

  return GTK_WIDGET (g_object_new (PT_TYPE_TIMESTAMP_BUTTON,
                                   "timestamp", timestamp,
                                   NULL));
}
