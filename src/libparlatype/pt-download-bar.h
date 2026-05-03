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

#define PT_TYPE_DOWNLOAD_BAR (pt_download_bar_get_type ())
G_DECLARE_FINAL_TYPE (PtDownloadBar, pt_download_bar, PT, DOWNLOAD_BAR, GtkWidget)

gboolean        pt_download_bar_get_revealed  (PtDownloadBar *self);

void            pt_download_bar_set_revealed     (PtDownloadBar *self,
                                                  gboolean       revealed);

char           *pt_download_bar_get_name         (PtDownloadBar *self);

void            pt_download_bar_set_name         (PtDownloadBar *self,
                                                  char          *name);

int             pt_download_bar_get_current      (PtDownloadBar *self);

void            pt_download_bar_set_current      (PtDownloadBar *self,
                                                  int            current);

void            pt_download_bar_set_total        (PtDownloadBar *self,
                                                  int            total);

void            pt_download_bar_set_cancellable  (PtDownloadBar *self,
                                                  GCancellable  *cancellable);

PtDownloadBar  *pt_download_bar_new              (void);

G_END_DECLS
