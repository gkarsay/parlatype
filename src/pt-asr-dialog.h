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


#ifndef PT_ASR_DIALOG_H
#define PT_ASR_DIALOG_H

#include "config.h"
#include <gtk/gtk.h>
#include <pt-config.h>

#define PT_TYPE_ASR_DIALOG              (pt_asr_dialog_get_type())
#define PT_ASR_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_ASR_DIALOG, PtAsrDialog))

typedef struct _PtAsrDialog		PtAsrDialog;
typedef struct _PtAsrDialogClass	PtAsrDialogClass;
typedef struct _PtAsrDialogPrivate	PtAsrDialogPrivate;

struct _PtAsrDialog
{
	GtkWindow dialog;

	/*< private > */
	PtAsrDialogPrivate *priv;
};

struct _PtAsrDialogClass
{
	GtkWindowClass parent_class;
};


GType		pt_asr_dialog_get_type		(void) G_GNUC_CONST;

void		pt_asr_dialog_set_config	(PtAsrDialog *dlg,
						 PtConfig    *config);

PtAsrDialog 	*pt_asr_dialog_new		(GtkWindow *win);

#endif
