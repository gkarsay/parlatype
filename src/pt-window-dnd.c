/* Copyright (C) Gabor Karsay 2016 <gabor.karsay@gmx.at>
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
#include <gtk/gtk.h>
#include "pt-window.h"
#include "pt-window-private.h"
#include "pt-window-dnd.h"


static gboolean
handle_uri (PtWindow *win,
            gchar    *data)
{
	GFile   *file;
	gchar  **split = NULL;
	gchar   *uri;
	gboolean success = FALSE;

	if (g_regex_match_simple ("^file:///.*", data, 0, 0)) {

		/* Nautilus gives us a \r\n separated list */
		split = g_strsplit (data, "\r", 0);
		file = g_file_new_for_uri (split[0]);

		if (g_file_query_exists (file, NULL)) {
			pt_window_open_file (win, split[0]);
			success = TRUE;
		}

		g_object_unref (file);
		g_strfreev (split);

		return success;
	}


	if (g_regex_match_simple ("^/.*/.*", data, 0, 0)) {

		file = g_file_new_for_path (data);

		if (g_file_query_exists (file, NULL)) {
			uri = g_file_get_uri (file);
			pt_window_open_file (win, uri);
			g_free (uri);
			success = TRUE;
		}

		g_object_unref (file);

		return success;
	}

	return FALSE;
}

static gboolean
handle_filename (PtWindow *win,
                 gchar    *data)
{
	gboolean       success;
	GList	      *list;
	GtkRecentInfo *info;
	gchar	      *uri;

	success = FALSE;

	list = gtk_recent_manager_get_items (win->priv->recent);
	for (list = g_list_first (list); list; list = g_list_next (list)) {
		info = (GtkRecentInfo *) list->data;
		if (g_strcmp0 (gtk_recent_info_get_display_name (info), data) == 0) {
			uri = g_strdup (gtk_recent_info_get_uri (info));
			success = TRUE;
			break;
		}
	}
	g_list_free_full (list, (GDestroyNotify) gtk_recent_info_unref);

	if (success) {
		/* GStreamer has problems with uris from recent chooser,
		   as a workaround use GFile magic */
		GFile *file = g_file_new_for_uri (uri);
		gchar *tmp = g_file_get_uri (file);
		pt_window_open_file (PT_WINDOW (win), tmp);
		g_free (tmp);
		g_free (uri);
		g_object_unref (file);
	}

	return success;
}

static gboolean
handle_dnd_data (PtWindow *win,
                 gchar    *data)
{
	g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	                  "MESSAGE", "Received drag and drop: '%s'", data);

	if (pt_player_goto_timestamp (win->player, data))
		return TRUE;

	if (handle_uri (win, data))
		return TRUE;

	if (handle_filename (win, data))
		return TRUE;

	return FALSE;
}


static gboolean
drop_cb (GtkDropTarget *self,
         GValue        *value,
         gdouble        x,
         gdouble        y,
         gpointer       user_data)
{
	PtWindow *win = PT_WINDOW (user_data);

	if (!G_VALUE_HOLDS_STRING(value))
		return FALSE;

	return handle_dnd_data (win, (void*) g_value_get_string (value));
}

void
pt_window_setup_dnd (PtWindow *win)
{
	/* The whole window is a destination for drops */

	GtkDropTarget *target;
	target = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_COPY);
	gtk_widget_add_controller (GTK_WIDGET (win), GTK_EVENT_CONTROLLER (target));
	g_signal_connect (target, "drop", G_CALLBACK (drop_cb), win);
}
