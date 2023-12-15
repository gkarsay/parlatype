/* Copyright (C) Gabor Karsay 2023 <gabor.karsay@gmx.at>
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
#include "pt-prefs-info-row.h"

struct _PtPrefsInfoRow
{
  AdwActionRow parent;

  GtkWidget *info_label;
};

G_DEFINE_FINAL_TYPE (PtPrefsInfoRow, pt_prefs_info_row, ADW_TYPE_ACTION_ROW)

void
pt_prefs_info_row_set_title (PtPrefsInfoRow *self,
                             gchar *title)
{
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);
}

void
pt_prefs_info_row_set_info (PtPrefsInfoRow *self,
                            gchar *info)
{
  gtk_label_set_text (GTK_LABEL (self->info_label), info);
}

static void
pt_prefs_info_row_init (PtPrefsInfoRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pt_prefs_info_row_class_init (PtPrefsInfoRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/parlatype/parlatype/prefs-info-row.ui");
  gtk_widget_class_bind_template_child (widget_class, PtPrefsInfoRow, info_label);
}

PtPrefsInfoRow *
pt_prefs_info_row_new (gchar *title,
                       gchar *info)
{
  PtPrefsInfoRow *self = g_object_new (PT_TYPE_PREFS_INFO_ROW, NULL);
  pt_prefs_info_row_set_title (self, title);
  pt_prefs_info_row_set_info (self, info);
  return self;
}
