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

#include <gio/gio.h>
#include <gst/gst.h>
#include "pt-media-info.h"

void               _pt_media_info_update_tags (PtMediaInfo *self,
                                               GstTagList  *tags);

void               _pt_media_info_update_caps (PtMediaInfo *self,
                                               GstCaps     *caps);

PtMediaInfo*       _pt_media_info_new         (void);
