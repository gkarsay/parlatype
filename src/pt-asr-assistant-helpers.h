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


#ifndef PT_ASR_ASSISTANT_HELPERS_H
#define PT_ASR_ASSISTANT_HELPERS_H

#include "config.h"
#include "pt-asr-assistant.h"

typedef struct {
	GPtrArray *lms;
	GPtrArray *dicts;
	GPtrArray *hmms;
} SearchResult;


void		search_result_free	(SearchResult *r);

gpointer	_pt_asr_assistant_recursive_search_finish (
					PtAsrAssistant  *self,
					GAsyncResult    *result,
					GError         **error);

void		_pt_asr_assistant_recursive_search_async (
					PtAsrAssistant      *self,
					GFile               *folder,
					GAsyncReadyCallback  callback,
					gpointer             user_data);

#endif
