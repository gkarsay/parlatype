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

#include <glib-object.h>

G_BEGIN_DECLS

#define PTP_TYPE_POCKETSPHINX (ptp_pocketsphinx_get_type())

G_DECLARE_FINAL_TYPE (PtpPocketsphinx, ptp_pocketsphinx, PTP, POCKETSPHINX, GObject)

/*
 * Boxing of decoder.
 */
#define PS_DECODER_TYPE (ps_decoder_get_type ())
GType ps_decoder_get_type (void);

G_END_DECLS
