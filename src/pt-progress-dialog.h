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


#ifndef PT_PROGRESS_DIALOG_H
#define PT_PROGRESS_DIALOG_H


#define PT_TYPE_PROGRESS_DIALOG		(pt_progress_dialog_get_type())
#define PT_PROGRESS_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_PROGRESS_DIALOG, PtProgressDialog))
#define PT_IS_PROGRESS_DIALOG(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_PROGRESS_DIALOG))

typedef struct _PtProgressDialog	PtProgressDialog;
typedef struct _PtProgressDialogClass	PtProgressDialogClass;
typedef struct _PtProgressDialogPrivate	PtProgressDialogPrivate;

struct _PtProgressDialog 
{
	GtkMessageDialog dialog;
	
	/*< private > */
	PtProgressDialogPrivate *priv;
};

struct _PtProgressDialogClass 
{
	GtkMessageDialogClass parent_class;
};


GType		pt_progress_dialog_get_type	(void) G_GNUC_CONST;

void		pt_progress_dialog_set_progress	(PtProgressDialog *dlg,
						 gdouble	   progress);

PtProgressDialog	*pt_progress_dialog_new	(GtkWindow *win);

G_END_DECLS

#endif
