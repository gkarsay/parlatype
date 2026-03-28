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

/**
 * PtAsrIface:
 *
 * Allows plugins to ...
 */

#include "config.h"

#include "pt-asr-iface.h"

G_DEFINE_INTERFACE (PtAsrIface, pt_asr_iface, G_TYPE_OBJECT)

static void
pt_asr_iface_default_init (PtAsrIfaceInterface *iface)
{
}

/**
 * pt_asr_iface_load:
 * @self: a #PtAsrIface
 * @player: a #PtPlayer
 * @text_view: a #PtTextView
 *
 * ASR plugins should prepare for speech recognition here
 */
void
pt_asr_iface_load (PtAsrIface *self,
                   PtPlayer   *player,
                   PtTextView *text_view)
{
  g_return_if_fail (PT_IS_ASR_IFACE (self));
  g_return_if_fail (PT_IS_PLAYER (player));
  g_return_if_fail (PT_IS_TEXT_VIEW (text_view));

  if (PT_ASR_IFACE_GET_IFACE (self)->load)
    PT_ASR_IFACE_GET_IFACE (self)->load (self, player, text_view);
}

/**
 * pt_asr_iface_unload:
 * @self: an #PtAsrIface
 *
 * Plugins should undo loading here.
 */
void
pt_asr_iface_unload (PtAsrIface *self)
{
  g_return_if_fail (PT_IS_ASR_IFACE (self));

  if (PT_ASR_IFACE_GET_IFACE (self)->unload)
    PT_ASR_IFACE_GET_IFACE (self)->unload (self);
}
