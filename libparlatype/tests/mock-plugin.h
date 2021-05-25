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


#ifndef MOCK_PLUGIN_H
#define MOCK_PLUGIN_H

#include <gst/gst.h>

#define MOCK_TYPE_PLUGIN	(mock_plugin_get_type())
#define MOCK_PLUGIN(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), MOCK_TYPE_PLUGIN, MockPlugin))
#define MOCK_IS_PLUGIN(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), MOCK_TYPE_PLUGIN))

typedef struct _MockPlugin		MockPlugin;
typedef struct _MockPluginClass		MockPluginClass;
typedef struct _MockPluginPrivate	MockPluginPrivate;

struct _MockPlugin
{
	GstElement parent;
	MockPluginPrivate *priv;
};

struct _MockPluginClass
{
	GstElementClass parent_class;
};


GType		mock_plugin_get_type		(void) G_GNUC_CONST;
gboolean	mock_plugin_register		(void);
MockPlugin*	mock_plugin_new			(void);

#endif
