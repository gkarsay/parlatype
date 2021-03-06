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


#ifndef PT_POSITION_MANAGER_H
#define PT_POSITION_MANAGER_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define PT_TYPE_POSITION_MANAGER	(pt_position_manager_get_type())
#define PT_POSITION_MANAGER(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_POSITION_MANAGER, PtPositionManager))
#define PT_IS_POSITION_MANAGER(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), PT_TYPE_POSITION_MANAGER))

typedef struct _PtPositionManager	 PtPositionManager;
typedef struct _PtPositionManagerClass   PtPositionManagerClass;
typedef struct _PtPositionManagerPrivate PtPositionManagerPrivate;

/**
 * PtPositionManager:
 *
 * The #PtPositionManager contains only private fields and should not be directly accessed.
 */
struct _PtPositionManager
{
	GObject parent;
};

struct _PtPositionManagerClass
{
	GObjectClass parent_class;
};


GType			pt_position_manager_get_type		(void) G_GNUC_CONST;

void			pt_position_manager_save		(PtPositionManager *self,
								 GFile *file,
								 gint64 pos);

gint64			pt_position_manager_load		(PtPositionManager *self,
								 GFile *file);

PtPositionManager*	pt_position_manager_new			(void);

G_END_DECLS

#endif
