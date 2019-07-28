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
#include <pt-wavedata.h>
#include "pt-asr-output.h"
#include "pt-asr-settings.h"

struct _PtWindowPrivate
{
	GSettings	 *editor;
	PtAsrSettings    *asr_settings;
	PtAsrOutput      *output;
	GtkRecentManager *recent;
	GtkAccelGroup    *accels;

	GtkWidget  *progress_dlg;
	gint	    progress_handler_id;

	guint       output_handler_id1;
	guint       output_handler_id2;

	GtkClipboard *clip;
	gulong        clip_handler_id;

	/* Headerbar widgets */
	GtkWidget  *button_open;
	GtkWidget  *primary_menu_button;

	/* Main window widgets */
	GtkWidget  *button_play;
	GtkWidget  *button_fast_back;	  // not used
	GtkWidget  *button_fast_forward;  // not used
	GtkWidget  *button_jump_back;
	GtkWidget  *button_jump_forward;
	GtkWidget  *volumebutton;
	GtkWidget  *pos_menu_button;
	GtkWidget  *pos_label;
	GMenuItem  *go_to_timestamp;
	GtkWidget  *speed_scale;
	GtkWidget  *waveviewer;
	GMenuModel *secondary_menu;

	PtWavedata *wavedata;

	gint64      last_time;	// last time to compare if it changed

	gint	    timer;
	gdouble	    speed;
};

#endif
