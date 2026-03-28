/* Copyright 2021 Gabor Karsay <gabor.karsay@gmx.at>
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

#define GETTEXT_PACKAGE GETTEXT_LIB

#include "config.h"

#include "gst-helpers.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gst/gst.h>

GstElement *
_pt_make_element (gchar   *factoryname,
                  gchar   *name,
                  GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  GstElement *result;

  result = gst_element_factory_make (factoryname, name);
  if (!result)
    {
      if (error == NULL)
        {
          g_log_structured (
              G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "MESSAGE",
              /* Translators: %s is replaced with the plugins name */
              _ ("Failed to load plugin “%s”."), factoryname);
        }
      else
        {
          g_set_error (error,
                       GST_CORE_ERROR,
                       GST_CORE_ERROR_MISSING_PLUGIN,
                       _ ("Failed to load plugin “%s”."), factoryname);
        }
    }

  return result;
}

GstPadLinkReturn
_pt_pad_link (GstPad *source,
              GstPad *sink)
{
  GstPadLinkReturn ret;

  ret = gst_pad_link (source, sink);
  if (ret == GST_PAD_LINK_OK)
    return ret;

  char      *srcname = gst_pad_get_name (source);
  char      *sinkname = gst_pad_get_name (sink);
  GstObject *srcparent = gst_pad_get_parent (source);
  GstObject *sinkparent = gst_pad_get_parent (sink);
  GstState   srcparent_state, sinkparent_state;
  gst_element_get_state (GST_ELEMENT (srcparent), &srcparent_state, NULL, 0);
  gst_element_get_state (GST_ELEMENT (sinkparent), &sinkparent_state, NULL, 0);
  GstCaps *srccaps_current = gst_pad_get_current_caps (source);
  GstCaps *sinkcaps_current = gst_pad_get_current_caps (sink);
  GstCaps *sinkcaps_allowed = gst_pad_get_allowed_caps (sink);

  GST_WARNING ("linking failed: %s", gst_pad_link_get_name (ret));
  GST_WARNING ("source pad \"%s\", parent \"%s\" in state %s", srcname, GST_OBJECT_NAME (srcparent), gst_element_state_get_name (srcparent_state));
  GST_WARNING ("sink pad \"%s\", parent \"%s\" in state %s", sinkname, GST_OBJECT_NAME (sinkparent), gst_element_state_get_name (sinkparent_state));
  GST_WARNING ("source pad current caps: %" GST_PTR_FORMAT, srccaps_current);
  GST_WARNING ("sink pad current caps: %" GST_PTR_FORMAT, sinkcaps_current);
  GST_WARNING ("sink pad allowed caps: %" GST_PTR_FORMAT, sinkcaps_allowed);

  g_free (srcname);
  g_free (sinkname);
  gst_object_unref (srcparent);
  gst_object_unref (sinkparent);
  if (srccaps_current != NULL)
    gst_caps_unref (srccaps_current);
  if (sinkcaps_current != NULL)
    gst_caps_unref (sinkcaps_current);
  if (sinkcaps_allowed != NULL)
    gst_caps_unref (sinkcaps_allowed);

  return ret;
}
