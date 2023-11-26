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

#ifndef PT_GOTO_DIALOG_H
#define PT_GOTO_DIALOG_H

#include "config.h"

#define PT_TYPE_GOTO_DIALOG (pt_goto_dialog_get_type ())
#define PT_GOTO_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_GOTO_DIALOG, PtGotoDialog))

typedef struct _PtGotoDialog PtGotoDialog;
typedef struct _PtGotoDialogClass PtGotoDialogClass;
typedef struct _PtGotoDialogPrivate PtGotoDialogPrivate;

struct _PtGotoDialog
{
  GtkDialog dialog;

  /*< private > */
  PtGotoDialogPrivate *priv;
};

struct _PtGotoDialogClass
{
  GtkDialogClass parent_class;
};

GType pt_goto_dialog_get_type (void) G_GNUC_CONST;

gint pt_goto_dialog_get_pos (PtGotoDialog *dlg);
void pt_goto_dialog_set_pos (PtGotoDialog *dlg,
                             gint seconds);
void pt_goto_dialog_set_max (PtGotoDialog *dlg,
                             gint seconds);

PtGotoDialog *pt_goto_dialog_new (GtkWindow *win);

#endif
