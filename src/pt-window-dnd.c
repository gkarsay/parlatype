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
handle_timestamp (PtWindow *win,
		  gchar *data)
{
	gboolean success;
	
	success = FALSE;
	
	/* TODO 
	   Is it a timestamp? 
	   If yes: jump to position */

	return success;
}

static gboolean
handle_uri (PtWindow *win,
	    gchar *data)
{
	gboolean  success;
	
	success = FALSE;

	/* TODO 
	   Is it a  valid uri or path? 
	   If yes: open file */

	return success;
}

static gboolean
handle_filename (PtWindow *win,
		 gchar *data)
{
	gboolean success;
	
	success = FALSE;
	
	/* TODO 
	   Is it a filename in the recent files list? 
	   If yes: open file */

	return success;
}

static gboolean
handle_dnd_data (PtWindow *win,
		 gchar    *data)
{
	g_debug ("Received drag and drop: '%s'", data);

	if (handle_timestamp (win, data))
		return TRUE;

	if (handle_uri (win, data))
		return TRUE;

	if (handle_filename (win, data))
		return TRUE;

	return FALSE;
}

static gboolean
win_dnd_drop_cb (GtkWidget *widget,
		 GdkDragContext *context,
		 gint x,
		 gint y,
		 guint time,
		 gpointer user_data)
{
	gboolean  is_valid_drop_site;
	GdkAtom   target_type;
	GList	 *iterate;
	gboolean  have_string;

	/* Check to see if (x,y) is a valid drop site within widget */
	is_valid_drop_site = TRUE;
	have_string = FALSE;

	/* Choose the best target type */
	iterate = gdk_drag_context_list_targets (context);
	while (iterate != NULL) {
		if (iterate->data == gdk_atom_intern ("STRING", TRUE)) {
			have_string = TRUE;
			break;
		}
		iterate = g_list_next (iterate);
	}

	if (have_string) {
		target_type = gdk_atom_intern ("STRING", TRUE);

                /* Request the data from the source. */
                gtk_drag_get_data (widget,	/* will receive 'drag-data-received' signal */
				   context,	/* represents the current state of the DnD */
				   target_type,	/* the target type we want */
				   time);            /* time stamp */
        } else {
	        /* No target offered by source => error */
                is_valid_drop_site = FALSE;
        }

        return  is_valid_drop_site;
}

static void
win_dnd_received_cb (GtkWidget *widget,
		     GdkDragContext *context,
		     int x,
		     int y,
		     GtkSelectionData *seldata,
		     guint info,
		     guint time,
		     gpointer user_data)
{
	PtWindow *win = user_data;
	gchar	 *data;
	gboolean  success;
	GError 	 *error = NULL;

	if ((seldata == NULL) ||
	    (gtk_selection_data_get_length (seldata) == 0) ||
	    (info != TARGET_STRING)) {
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

void
pt_window_setup_dnd (PtWindow *win)
{
	gtk_drag_dest_set (GTK_WIDGET (win),
				GTK_DEST_DEFAULT_MOTION,
				drag_target_string, 1,
				GDK_ACTION_COPY);

	g_signal_connect (GTK_WIDGET (win),
				"drag_data_received",
				G_CALLBACK (win_dnd_received_cb),
				win);

	g_signal_connect (GTK_WIDGET (win),
				"drag_drop",
				G_CALLBACK (win_dnd_drop_cb),
				win);
}


