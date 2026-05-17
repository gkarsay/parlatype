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

#if !defined(__PARLATYPE_H_INSIDE__) && !defined(PARLATYPE_COMPILATION)
#error "Only <parlatype.h> can be included directly."
#endif

#include <gio/gio.h>
#include "pt-downloader.h"

G_BEGIN_DECLS

#define PT_TYPE_DOWNLOAD (pt_download_get_type ())
G_DECLARE_FINAL_TYPE (PtDownload, pt_download, PT, DOWNLOAD, GObject)

guint64       pt_download_get_elapsed_time (PtDownload *self);

void          pt_download_cancel           (PtDownload *self);

void          pt_download_start            (PtDownload *self);

PtDownloader *pt_download_get_downloader   (PtDownload *self);

PtDownload   *pt_download_new              (char       *display_name,
                                            char       *plugin_name);

G_END_DECLS
