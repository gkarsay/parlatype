/* Copyright 2024 Gabor Karsay <gabor.karsay@gmx.at>
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

#if !defined(__PARLATYPE_H_INSIDE__) && !defined(PARLATYPE_COMPILATION)
#error "Only <parlatype.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define PT_TYPE_MEDIA_INFO (pt_media_info_get_type ())
G_DECLARE_FINAL_TYPE (PtMediaInfo, pt_media_info, PT, MEDIA_INFO, GObject)

gchar*   pt_media_info_get_album   (PtMediaInfo *self);
GStrv    pt_media_info_get_artist  (PtMediaInfo *self);
gchar*   pt_media_info_get_title   (PtMediaInfo *self);

gchar*   pt_media_info_dump_caps   (PtMediaInfo *self);
gchar*   pt_media_info_dump_tags   (PtMediaInfo *self);

G_END_DECLS
