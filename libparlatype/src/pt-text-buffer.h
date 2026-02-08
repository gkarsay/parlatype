/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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

#include <gtk/gtk.h>

#define PT_TYPE_TEXT_BUFFER (pt_text_buffer_get_type ())
G_DECLARE_FINAL_TYPE (PtTextBuffer, pt_text_buffer, PT, TEXT_BUFFER, GtkTextBuffer)

#define PT_TEXT_UNKNOWN_CHAR 0xFFFC
/* timestamp delimiter number sign: # */
#define PT_TEXT_TIMESTAMP_DELIMITER 0x0023

void             pt_text_buffer_insert_range    (PtTextBuffer      *buffer,
                                                 GtkTextIter       *iter,
                                                 const GtkTextIter *start,
                                                 const GtkTextIter *end);

void             pt_text_buffer_copy_clipboard  (PtTextBuffer *buffer,
                                                 GdkClipboard *clipboard);

void             pt_text_buffer_cut_clipboard   (PtTextBuffer *buffer,
                                                 GdkClipboard *clipboard,
                                                 gboolean      default_editable);

void             pt_text_buffer_paste_clipboard (PtTextBuffer *buffer,
                                                 GdkClipboard *clipboard);

PtTextBuffer    *pt_text_buffer_new             (GtkTextTagTable *tag_table);
