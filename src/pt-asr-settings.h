/* Copyright (C) Gabor Karsay 2018 <gabor.karsay@gmx.at>
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


#ifndef PT_ASR_SETTINGS_H
#define PT_ASR_SETTINGS_H

#include <gio/gio.h>
#include "pt-player.h"

#define PT_TYPE_ASR_SETTINGS	(pt_asr_settings_get_type())
#define PT_ASR_SETTINGS(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_ASR_SETTINGS, PtAsrSettings))
#define PT_IS_ASR_SETTINGS(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_ASR_SETTINGS))

typedef struct _PtAsrSettings		PtAsrSettings;
typedef struct _PtAsrSettingsClass	PtAsrSettingsClass;
typedef struct _PtAsrSettingsPrivate	PtAsrSettingsPrivate;

/**
 * PtAsrSettings:
 *
 * #PtAsrSettings contains only private fields and should not be directly accessed.
 */
struct _PtAsrSettings
{
	GObject parent;

	/*< private > */
	PtAsrSettingsPrivate *priv;
};

struct _PtAsrSettingsClass
{
	GObjectClass parent_class;
};


GType		pt_asr_settings_get_type	(void) G_GNUC_CONST;

gboolean	pt_asr_settings_set_string	(PtAsrSettings *settings,
                                                 gchar *id,
                                                 gchar *field,
                                                 gchar *string);
gchar*		pt_asr_settings_get_string	(PtAsrSettings *settings,
                                                 gchar *id,
                                                 gchar *field);

gboolean	pt_asr_settings_set_int		(PtAsrSettings *settings,
                                                 gchar *id,
                                                 gchar *field,
                                                 gint   value);
int		pt_asr_settings_get_int		(PtAsrSettings *settings,
                                                 gchar *id,
                                                 gchar *field);

gboolean	pt_asr_settings_set_boolean	(PtAsrSettings *settings,
                                                 gchar *id,
                                                 gchar *field,
                                                 gboolean value);
gboolean	pt_asr_settings_get_boolean	(PtAsrSettings *settings,
                                                 gchar *id,
                                                 gchar *field);

/*gboolean	pt_asr_settings_name_is_unique	(PtAsrSettings *settings,
                                                 gchar *name);*/

gchar**		pt_asr_settings_get_configs     (PtAsrSettings *settings);

gboolean	pt_asr_settings_have_configs    (PtAsrSettings *settings);

gchar*		pt_asr_settings_add_config	(PtAsrSettings *settings,
                                                 gchar *name,
                                                 gchar *type);

gboolean	pt_asr_settings_remove_config	(PtAsrSettings *settings,
                                                 gchar *id);

gboolean	pt_asr_settings_apply_config	(PtAsrSettings *settings,
                                                 gchar *id,
						 PtPlayer *player);

PtAsrSettings*	pt_asr_settings_new		(gchar *settings_file);

#endif
