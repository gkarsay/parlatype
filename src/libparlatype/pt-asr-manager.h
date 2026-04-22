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

#if !defined(__PARLATYPE_H_INSIDE__) && !defined(PARLATYPE_COMPILATION)
#error "Only <parlatype.h> can be included directly."
#endif

#include <gio/gio.h>
#include "pt-config.h"
#include "pt-player.h"
#include "pt-text-view.h"

G_BEGIN_DECLS

#define PT_TYPE_ASR_MANAGER (pt_asr_manager_get_type ())
G_DECLARE_FINAL_TYPE (PtAsrManager, pt_asr_manager, PT, ASR_MANAGER, GObject)


void          pt_asr_manager_start_transcribing     (PtAsrManager       *self);

void          pt_asr_manager_stop_transcribing      (PtAsrManager       *self);

gboolean      pt_asr_manager_load_plugin_finish     (PtAsrManager       *self,
                                                     GAsyncResult       *result,
                                                     GError            **error);

void          pt_asr_manager_load_plugin_async      (PtAsrManager       *self,
                                                     GCancellable       *cancellable,
                                                     GAsyncReadyCallback callback,
                                                     gpointer            user_data);

gboolean      pt_asr_manager_plugin_is_available    (PtAsrManager       *self,
                                                     PtConfig           *config);

PtConfig     *pt_asr_manager_get_config             (PtAsrManager       *self);

gboolean      pt_asr_manager_set_config             (PtAsrManager       *self,
                                                     PtConfig           *config);

gboolean      pt_asr_manager_have_config            (PtAsrManager       *self);

gboolean      pt_asr_manager_have_asr               (PtAsrManager       *self);

PtAsrManager *pt_asr_manager_new                    (PtPlayer           *player,
                                                     PtTextView         *text_view,
                                                     GtkWidget          *asr_controls);

G_END_DECLS
