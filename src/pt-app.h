/* Copyright (C) Gabor Karsay 2016, 2019 <gabor.karsay@gmx.at>
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

#ifndef PT_APP_H
#define PT_APP_H

#include "config.h"
#include <gtk/gtk.h>
#include "pt-asr-settings.h"

#define PT_APP_TYPE		(pt_app_get_type())
#define PT_APP(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_APP_TYPE, PtApp))
#define PT_IS_APP(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_APP_TYPE))

typedef struct _PtApp		PtApp;
typedef struct _PtAppClass	PtAppClass;
typedef struct _PtAppPrivate	PtAppPrivate;

struct _PtApp
{
	GtkApplication parent;

	/*< private > */
	PtAppPrivate *priv;
};

struct _PtAppClass
{
	GtkApplicationClass parent_class;
};


GType		pt_app_get_type			(void) G_GNUC_CONST;

PtAsrSettings	*pt_app_get_asr_settings	(PtApp *app);

gboolean	pt_app_get_atspi		(PtApp *app);

gboolean	pt_app_get_asr			(PtApp *app);

PtApp		*pt_app_new			(void);

#endif
