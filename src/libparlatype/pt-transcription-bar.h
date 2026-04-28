/* Copyright 2026 Gabor Karsay <gabor.karsay@gmx.at>
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

#pragma once

#include "parlatype.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_TRANSCRIPTION_BAR (pt_transcription_bar_get_type ())
G_DECLARE_FINAL_TYPE (PtTranscriptionBar, pt_transcription_bar, PT, TRANSCRIPTION_BAR, GtkWidget)

gboolean            pt_transcription_bar_get_revealed  (PtTranscriptionBar *self);

void                pt_transcription_bar_set_revealed  (PtTranscriptionBar *self,
                                                        gboolean            revealed);

PtConfig           *pt_transcription_bar_get_config    (PtTranscriptionBar *self);

void                pt_transcription_bar_set_config    (PtTranscriptionBar *self,
                                                        PtConfig           *config);

PtTranscriptionBar *pt_transcription_bar_new           (void);

G_END_DECLS
