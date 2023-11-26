/* Copyright (C) Gabor Karsay 2017 <gabor.karsay@gmx.at>
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

#ifndef PT_WAVEVIEWER_SELECTION_H
#define PT_WAVEVIEWER_SELECTION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVEVIEWER_SELECTION (pt_waveviewer_selection_get_type ())
#define PT_WAVEVIEWER_SELECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_WAVEVIEWER_SELECTION, PtWaveviewerSelection))
#define PT_IS_WAVEVIEWER_SELECTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_WAVEVIEWER_SELECTION))
#define PT_WAVEVIEWER_SELECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), PT_TYPE_WAVEVIEWER_SELECTION, PtWaveviewerSelectionClass))
#define PT_IS_WAVEVIEWER_SELECTION_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass), PT_TYPE_WAVEVIEWER_SELECTION))

typedef struct _PtWaveviewerSelection PtWaveviewerSelection;
typedef struct _PtWaveviewerSelectionClass PtWaveviewerSelectionClass;
typedef struct _PtWaveviewerSelectionPrivate PtWaveviewerSelectionPrivate;

struct _PtWaveviewerSelection
{
  GtkDrawingArea parent;

  /*< private > */
  PtWaveviewerSelectionPrivate *priv;
};

struct _PtWaveviewerSelectionClass
{
  GtkDrawingAreaClass klass;
};

GType pt_waveviewer_selection_get_type (void) G_GNUC_CONST;
void pt_waveviewer_selection_set (PtWaveviewerSelection *self,
                                  gint start,
                                  gint end);
GtkWidget *pt_waveviewer_selection_new (void);

G_END_DECLS

#endif // PT_WAVEVIEWER_SELECTION_H
