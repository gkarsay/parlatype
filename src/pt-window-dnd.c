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
#include <string.h>	/* strlen */
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

	if (pt_player_goto_timestamp (win->priv->player, data))
		return TRUE;

	if (handle_uri (win, data))
		return TRUE;

	if (handle_filename (win, data))
		return TRUE;

	return FALSE;
}

static gboolean
win_dnd_drop_cb (GtkWidget     *widget,
                GdkDragContext *context,
                gint            x,
                gint            y,
                guint           time,
                gpointer        user_data)
{
	gboolean  is_valid_drop_site;
	GdkAtom   target_type;
	GList	 *iterate;
	gboolean  have_target;

	/* Check to see if (x,y) is a valid drop site within widget */
	is_valid_drop_site = TRUE;
	have_target = FALSE;

	/* Choose the best target type */
	iterate = gdk_drag_context_list_targets (context);
	while (iterate != NULL) {
		g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
			          "MESSAGE", "atom string: %s", gdk_atom_name (iterate->data));
		if (iterate->data == gdk_atom_intern ("STRING", TRUE)) {
			target_type = gdk_atom_intern ("STRING", TRUE);
			have_target = TRUE;
			break;
		}
		if (iterate->data == gdk_atom_intern ("UTF8_STRING", TRUE)) {
			target_type = gdk_atom_intern ("UTF8_STRING", TRUE);
			have_target = TRUE;
			break;
		}
		iterate = g_list_next (iterate);
	}

	if (have_target) {

		/* Request the data from the source. */
		gtk_drag_get_data (widget,	/* will receive “drag-data-received” signal */
				context,	/* represents the current state of the DnD */
				target_type,
				time);
	} else {
		/* No target offered by source => error */
		is_valid_drop_site = FALSE;
	}

	return  is_valid_drop_site;
}

static void
win_dnd_received_cb (GtkWidget        *widget,
                     GdkDragContext   *context,
                     int               x,
                     int               y,
                     GtkSelectionData *seldata,
                     guint             info,
                     guint             time,
                     gpointer       user_data)
{
	PtWindow *win = user_data;
	gchar	 *data;
	gboolean  success;

	if ((seldata == NULL) ||
	    (gtk_selection_data_get_length (seldata) == 0) ||
	    (info != TARGET_STRING && info != TARGET_UTF8_STRING)) {
		gtk_drag_finish (context,
				 FALSE,		/* success */
				 FALSE,		/* delete source */
				 time);
		return;
	}

	data = (gchar*) gtk_selection_data_get_data (seldata);
	success = handle_dnd_data (win, data);

	gtk_drag_finish (context,
			 success,
			 FALSE,		/* delete source */
			 time);
}

static gboolean
win_dnd_motion_cb (GtkWidget      *widget,
                   GdkDragContext *context,
                   gint            x,
                   gint            y,
                   guint           info,
                   guint           time,
                   gpointer        user_data)
{
	/* We handle this signal ourselves, because using GTK_DEST_DEFAULT_MOTION
	   drags with source LibreOffice show an icon indicating drag is not
	   possible. */

	GdkAtom target_atom;

	target_atom = gtk_drag_dest_find_target (widget, context, NULL);
	if (target_atom != GDK_NONE) {
		gdk_drag_status (context, GDK_ACTION_COPY, time);
		return TRUE;
	}

	gdk_drag_status (context, 0, time);

	return FALSE;
}

void
pt_window_setup_dnd (PtWindow *win)
{
	/* The whole window is a destination for timestamp drops */

	gtk_drag_dest_set (GTK_WIDGET (win),
				0,	/* no default behavior flags set, we do everything ourselves */
				drag_target_string,
				G_N_ELEMENTS (drag_target_string),
				GDK_ACTION_COPY);

	g_signal_connect (GTK_WIDGET (win),
				"drag_motion",
				G_CALLBACK (win_dnd_motion_cb),
				win);

	g_signal_connect (GTK_WIDGET (win),
				"drag_data_received",
				G_CALLBACK (win_dnd_received_cb),
				win);

	g_signal_connect (GTK_WIDGET (win),
				"drag_drop",
				G_CALLBACK (win_dnd_drop_cb),
				win);
}
