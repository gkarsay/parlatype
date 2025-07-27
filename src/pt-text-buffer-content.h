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
#include "pt-text-buffer.h"

#define PT_TYPE_TEXT_BUFFER_CONTENT (pt_text_buffer_content_get_type ())
G_DECLARE_FINAL_TYPE (PtTextBufferContent, pt_text_buffer_content, PT, TEXT_BUFFER_CONTENT, GdkContentProvider)

GdkContentProvider *pt_text_buffer_content_new (PtTextBuffer *buffer);
