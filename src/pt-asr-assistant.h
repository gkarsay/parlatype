/* Copyright (C) Gabor Karsay 2019 <gabor.karsay@gmx.at>
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


#ifndef PT_ASR_ASSISTANT_H
#define PT_ASR_ASSISTANT_H

#include "config.h"
#include "pt-asr-settings.h"

#define PT_TYPE_ASR_ASSISTANT              (pt_asr_assistant_get_type())
#define PT_ASR_ASSISTANT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_ASR_ASSISTANT, PtAsrAssistant))

typedef struct _PtAsrAssistant		PtAsrAssistant;
typedef struct _PtAsrAssistantClass	PtAsrAssistantClass;
typedef struct _PtAsrAssistantPrivate	PtAsrAssistantPrivate;

struct _PtAsrAssistant
{
	GtkAssistant assistant;

	/*< private > */
	PtAsrAssistantPrivate *priv;
};

struct _PtAsrAssistantClass
{
	GtkAssistantClass parent_class;
};


GType		pt_asr_assistant_get_type	(void) G_GNUC_CONST;

gchar*		pt_asr_assistant_get_name	(PtAsrAssistant *self);

gchar*		pt_asr_assistant_save_config	(PtAsrAssistant *self,
						 PtAsrSettings  *settings);

PtAsrAssistant 	*pt_asr_assistant_new		(GtkWindow     *win);

#endif
