/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
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


#ifndef PT_CONFIG_ROW_H
#define PT_CONFIG_ROW_H

#include <gio/gio.h>
#include <parlatype.h>

#define PT_TYPE_CONFIG_ROW	(pt_config_row_get_type())
#define PT_CONFIG_ROW(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_CONFIG_ROW, PtConfigRow))
#define PT_IS_CONFIG_ROW(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_CONFIG_ROW))

typedef struct _PtConfigRow		PtConfigRow;
typedef struct _PtConfigRowClass	PtConfigRowClass;
typedef struct _PtConfigRowPrivate	PtConfigRowPrivate;

/**
 * PtConfigRow:
 *
 * #PtConfigRow contains only private fields and should not be directly accessed.
 */
struct _PtConfigRow
{
	GtkListBoxRow parent;

	/*< private > */
	PtConfigRowPrivate *priv;
};

struct _PtConfigRowClass
{
	GtkListBoxRowClass parent_class;
};


GType		pt_config_row_get_type		(void) G_GNUC_CONST;

void		pt_config_row_set_active	(PtConfigRow *row,
						 gboolean     active);
gboolean	pt_config_row_get_active	(PtConfigRow *row);
gboolean	pt_config_row_is_installed	(PtConfigRow *row);
void		pt_config_row_set_supported	(PtConfigRow *row,
						 gboolean     supported);
gboolean	pt_config_row_get_supported	(PtConfigRow *row);
PtConfigRow*	pt_config_row_new		(PtConfig *config);

#endif
