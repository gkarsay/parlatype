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
#include <pt-player.h>

struct _PtWindowPrivate
{
	GSettings	 *editor;
	GtkRecentManager *recent;
	PtConfig         *asr_config;

	GtkClipboard *clip;
	gulong        clip_handler_id;

	/* Headerbar widgets */
	GtkWidget  *button_open;
	GtkWidget  *primary_menu_button;

	/* Main window widgets */
	GtkWidget  *controls_row_box;
	GtkWidget  *controls_box;
	GtkWidget  *progress;
	GtkWidget  *button_play;
	GtkWidget  *button_jump_back;
	GtkWidget  *button_jump_forward;
	GtkWidget  *volumebutton;
	GStrv       vol_icons;
	GtkGesture *vol_event;
	GtkWidget  *pos_menu_button;
	GtkWidget  *pos_label;
	GMenuItem  *go_to_timestamp;
	GtkWidget  *speed_scale;
	GtkWidget  *waveviewer;

	GMenuModel *primary_menu;
	GMenuModel *secondary_menu;
	GMenu      *asr_menu;
	GMenuItem  *asr_menu_item1;
	GMenuItem  *asr_menu_item2;
	gboolean    asr;

	gint64      last_time;	// last time to compare if it changed

	gint	    timer;
	gdouble	    speed;
};

#endif
