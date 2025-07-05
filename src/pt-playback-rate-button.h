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

#include <adwaita.h>

#define PT_TYPE_PLAYBACK_RATE_BUTTON (pt_playback_rate_button_get_type ())
G_DECLARE_FINAL_TYPE (PtPlaybackRateButton, pt_playback_rate_button, PT, PLAYBACK_RATE_BUTTON, AdwBin)

GtkAdjustment        *pt_playback_rate_button_get_adjustment (PtPlaybackRateButton *self);

PtPlaybackRateButton *pt_playback_rate_button_new            (void);
