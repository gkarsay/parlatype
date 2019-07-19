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

/* This is the preferences dialog */

#ifndef PT_PREFERENCES_H
#define PT_PREFERENCES_H

#include "config.h"

#define PT_TYPE_PREFERENCES_DIALOG              (pt_preferences_dialog_get_type())
#define PT_PREFERENCES_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_PREFERENCES_DIALOG, PtPreferencesDialog))

typedef struct _PtPreferencesDialog		PtPreferencesDialog;
typedef struct _PtPreferencesDialogClass	PtPreferencesDialogClass;
typedef struct _PtPreferencesDialogPrivate	PtPreferencesDialogPrivate;

struct _PtPreferencesDialog 
{
	GtkDialog dialog;
	
	/*< private > */
	PtPreferencesDialogPrivate *priv;
};

struct _PtPreferencesDialogClass 
{
	GtkDialogClass parent_class;
};


GType		pt_preferences_dialog_get_type	(void) G_GNUC_CONST;

void		pt_show_preferences_dialog	(GtkWindow *parent);

#endif
