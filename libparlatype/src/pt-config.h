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


#ifndef PT_CONFIG_H
#define PT_CONFIG_H

#include <gio/gio.h>

#define PT_TYPE_CONFIG		(pt_config_get_type())
#define PT_CONFIG(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_CONFIG, PtConfig))
#define PT_IS_CONFIG(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), PT_TYPE_CONFIG))

typedef struct _PtConfig	PtConfig;
typedef struct _PtConfigClass	PtConfigClass;
typedef struct _PtConfigPrivate	PtConfigPrivate;

/**
 * PtConfig:
 *
 * #PtConfig contains only private fields and should not be directly accessed.
 */
struct _PtConfig
{
	GObject parent;

	/*< private > */
	PtConfigPrivate *priv;
};

struct _PtConfigClass
{
	GObjectClass parent_class;
};


GType		pt_config_get_type		(void) G_GNUC_CONST;

gchar*		pt_config_get_name		(PtConfig *config);
gboolean	pt_config_set_name		(PtConfig *config,
						 gchar    *name);
gchar*		pt_config_get_base_folder	(PtConfig *config);
gboolean	pt_config_set_base_folder	(PtConfig *config,
						 gchar    *name);
gchar*		pt_config_get_plugin		(PtConfig *config);
gchar*		pt_config_get_lang_code		(PtConfig *config);
gchar*		pt_config_get_lang_name		(PtConfig *config);
gchar*          pt_config_get_other             (PtConfig *config,
						 gchar    *key);

GFile*		pt_config_get_file		(PtConfig *config);
void            pt_config_set_file              (PtConfig *config,
						 GFile    *file);

gboolean	pt_config_apply			(PtConfig *config,
						 GObject  *plugin);
gboolean	pt_config_is_valid		(PtConfig *config);
gboolean	pt_config_is_installed		(PtConfig *config);
PtConfig*	pt_config_new			(GFile *file);

#endif
