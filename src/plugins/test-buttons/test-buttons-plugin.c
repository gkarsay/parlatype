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

#include "pt-window-addin.h"
#include "ptp-test-buttons.h"

#include <libpeas.h>
#include <parlatype.h>

void
ptp_test_buttons_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              PT_TYPE_WINDOW_ADDIN,
                                              PTP_TYPE_TEST_BUTTONS);
}
