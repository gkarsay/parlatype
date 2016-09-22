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

#ifndef PT_WINDOW_PRIVATE_H
#define PT_WINDOW_PRIVATE_H

#include "config.h"
#include <gtk/gtk.h>
#include <libparlatype/pt-player.h>

struct _PtWindowPrivate
{
	GSettings	 *editor;
	GDBusProxy	 *proxy;
	GtkRecentManager *recent;
	PtPlayer	 *player;

	GtkWidget  *progress_dlg;
	gint	    progress_handler_id;

	GtkWidget  *button_open;
	GtkWidget  *button_play;
	GtkWidget  *button_fast_back;	  // not used
	GtkWidget  *button_fast_forward;  // not used
	GtkWidget  *button_jump_back;
	GtkWidget  *button_jump_forward;
	GtkWidget  *volumebutton;
	GtkWidget  *pos_label;
	GtkWidget  *dur_label;
	GtkWidget  *time_scale;
	GtkWidget  *time_adj;
	GtkWidget  *speed_scale;
	GtkWidget  *main_grid;
	GtkWidget  *waveslider;

	gint	    timer;
	gdouble	    speed;

	gint	    pause;
	gint	    back;
	gint	    forward;

	guint	    owner_id;
};

#endif
