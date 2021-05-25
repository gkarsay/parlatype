/* Copyright (C) Gabor Karsay 2021 <gabor.karsay@gmx.at>
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

#include "config.h"
#define GETTEXT_PACKAGE GETTEXT_LIB
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include "gst-helpers.h"


GstElement*
_pt_make_element (gchar   *factoryname,
                  gchar   *name,
                  GError **error)
{
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	GstElement *result;

	result = gst_element_factory_make (factoryname, name);
	if (!result) {
		if (error == NULL) {
			g_log_structured (
				G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "MESSAGE",
				_("Failed to load plugin “%s”."), factoryname);
		} else {
			g_set_error (error,
			             GST_CORE_ERROR,
			             GST_CORE_ERROR_MISSING_PLUGIN,
			             _("Failed to load plugin “%s”."), factoryname);

		}
	}

	return result;
}
