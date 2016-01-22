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

#ifndef PT_WINDOW_DND_H
#define PT_WINDOW_DND_H

#include "config.h"
#include <gtk/gtk.h>
#include "pt-window.h"

/* Drag and Drop */
enum {
	TARGET_STRING,
};

static const GtkTargetEntry drag_target_string[] = {
	{ "STRING", GTK_TARGET_OTHER_APP, TARGET_STRING }
};


void	pt_window_setup_dnd	(PtWindow *win);

#endif
