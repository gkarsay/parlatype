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

struct _PtTimestampButton
{
  GtkButton parent_instance;
  char     *timestamp;
};

enum
{
  PROP_0,
  PROP_TIMESTAMP,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PtTimestampButton, pt_timestamp_button, GTK_TYPE_BUTTON)

static GParamSpec *properties[N_PROPS];

char *
pt_timestamp_button_get_timestamp (PtTimestampButton *self)
{
  g_return_val_if_fail (PT_IS_TIMESTAMP_BUTTON (self), NULL);

  return self->timestamp;
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
      g_assert (self->timestamp == NULL);
      self->timestamp = g_value_dup_string (value);
      // gtk_button_set_label (self->button, self->timestamp);
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
      g_value_set_string (value, self->timestamp);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
pt_timestamp_button_dispose (GObject *object)
{
  PtTimestampButton *self = PT_TIMESTAMP_BUTTON (object);

  g_free (self->timestamp);

  G_OBJECT_CLASS (pt_timestamp_button_parent_class)->dispose (object);
}

static void
pt_timestamp_button_constructed (GObject *object)
{
  PtTimestampButton *self = PT_TIMESTAMP_BUTTON (object);
  char              *label;

  /* Remove delimiters */
  if (g_str_has_prefix (self->timestamp, "#") || g_str_has_prefix (self->timestamp, "(") || g_str_has_prefix (self->timestamp, "["))
    {
      self->timestamp++;
      label = g_strdup_printf ("%.*s", (int) strlen (self->timestamp) - 1, self->timestamp);
      self->timestamp--;
    }
  else
    {
      label = g_strdup (self->timestamp);
    }

  gtk_button_set_label (GTK_BUTTON (self), label);
  g_free (label);

  G_OBJECT_CLASS (pt_timestamp_button_parent_class)->constructed (object);
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
  object_class->constructed = pt_timestamp_button_constructed;
  object_class->dispose = pt_timestamp_button_dispose;

  properties[PROP_TIMESTAMP] =
      g_param_spec_string ("timestamp", NULL, NULL,
                           "n/a",
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

GtkWidget *
pt_timestamp_button_new (char *timestamp)
{
  g_return_val_if_fail (timestamp != NULL, NULL);

  return GTK_WIDGET (g_object_new (PT_TYPE_TIMESTAMP_BUTTON,
                                   "timestamp", timestamp,
                                   NULL));
}
