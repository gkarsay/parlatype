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

/**
 * PtDownloader:
 *
 * Interface for plugins which download models and install them.
 */

#include "config.h"

#include "pt-downloader.h"

G_DEFINE_INTERFACE (PtDownloader, pt_downloader, G_TYPE_OBJECT)

static void
pt_downloader_default_init (PtDownloaderInterface *iface)
{
}

/**
 * pt_downloader_download_finish:
 * @self: a #PtDownloader
 * @result: result
 * @error: error location
 *
 * Finishes the operation.
 *
 * Returns: TRUE on success.
 */
gboolean
pt_downloader_download_finish (PtDownloader *self,
                               GAsyncResult *result,
                               GError      **error)
{
  PtDownloaderInterface *iface;

  g_return_val_if_fail (PT_IS_DOWNLOADER (self), FALSE);

  iface = PT_DOWNLOADER_GET_IFACE (self);
  g_return_val_if_fail (iface->download_finish != NULL, FALSE);

  return iface->download_finish (self, result, error);
}

/**
 * pt_downloader_download_async:
 * @self: a #PtDownloader
 * @cancellable: a cancellable
 * @callback: (scope async) (closure): a callback
 * @user_data: user data
 *
 * Starts the download operation. All parameters must have been set before
 * on properties. Plugins using this interface may have different sets
 * of parameters which are set by pt_config_configure_downnloader().
 */
void
pt_downloader_download_async (PtDownloader       *self,
                              GCancellable       *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer            user_data)

{
  PtDownloaderInterface *iface;

  g_return_if_fail (PT_IS_DOWNLOADER (self));

  iface = PT_DOWNLOADER_GET_IFACE (self);
  g_return_if_fail (iface->download_async != NULL);

  iface->download_async (self, cancellable, callback, user_data);
}
