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


#ifndef PT_PREFS_ASR_H
#define PT_PREFS_ASR_H

#include "config.h"

#define PT_TYPE_PREFS_ASR                     (pt_prefs_asr_get_type())
#define PT_PREFS_ASR(obj)                     (G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_PREFS_ASR, PtPrefsAsr))

typedef struct _PtPrefsAsr		PtPrefsAsr;
typedef struct _PtPrefsAsrClass		PtPrefsAsrClass;
typedef struct _PtPrefsAsrPrivate	PtPrefsAsrPrivate;

struct _PtPrefsAsr
{
	GtkBox parent;

	/*< private > */
	PtPrefsAsrPrivate *priv;
};

struct _PtPrefsAsrClass
{
	GtkBoxClass parent_class;
};


GType		pt_prefs_asr_get_type	(void) G_GNUC_CONST;

GtkWidget*	pt_prefs_asr_new	(GSettings *editor,
					 PtPlayer  *player);

#endif
