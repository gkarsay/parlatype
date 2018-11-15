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


#ifndef PT_ASR_OUTPUT_H
#define PT_ASR_OUTPUT_H

#include "config.h"

#define PT_TYPE_ASR_OUTPUT              (pt_asr_output_get_type())
#define PT_ASR_OUTPUT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), PT_TYPE_ASR_OUTPUT, PtAsrOutput))

typedef struct _PtAsrOutput		PtAsrOutput;
typedef struct _PtAsrOutputClass	PtAsrOutputClass;
typedef struct _PtAsrOutputPrivate	PtAsrOutputPrivate;

struct _PtAsrOutput 
{
	GObject parent;
	
	/*< private > */
	PtAsrOutputPrivate *priv;
};

struct _PtAsrOutputClass 
{
	GObjectClass parent_class;
};


GType		pt_asr_output_get_type		(void) G_GNUC_CONST;

void		pt_asr_output_hypothesis	(PtAsrOutput *self,
                                                 gchar       *string);

void		pt_asr_output_final		(PtAsrOutput *self,
                                                 gchar       *string);

gchar*          pt_asr_output_get_app_name	(PtAsrOutput *self);

void            pt_asr_output_search_app	(PtAsrOutput *self);

void            pt_asr_output_cancel_search	(PtAsrOutput *self);

void		pt_asr_output_reset		(PtAsrOutput *self);

PtAsrOutput 	*pt_asr_output_new		(void);

G_END_DECLS

#endif
