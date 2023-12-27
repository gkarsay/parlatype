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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PT_TYPE_WAVEVIEWER_RULER (pt_waveviewer_ruler_get_type ())
#define PT_WAVEVIEWER_RULER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PT_TYPE_WAVEVIEWER_RULER, PtWaveviewerRuler))
#define PT_IS_WAVEVIEWER_RULER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PT_TYPE_WAVEVIEWER_RULER))
#define PT_WAVEVIEWER_RULER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), PT_TYPE_WAVEVIEWER_RULER, PtWaveviewerRulerClass))
#define PT_IS_WAVEVIEWER_RULER_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass), PT_TYPE_WAVEVIEWER_RULER))

typedef struct _PtWaveviewerRuler PtWaveviewerRuler;
typedef struct _PtWaveviewerRulerClass PtWaveviewerRulerClass;
typedef struct _PtWaveviewerRulerPrivate PtWaveviewerRulerPrivate;

struct _PtWaveviewerRuler
{
  GtkDrawingArea parent;

  /*< private > */
  PtWaveviewerRulerPrivate *priv;
};

struct _PtWaveviewerRulerClass
{
  GtkDrawingAreaClass klass;
};

GType pt_waveviewer_ruler_get_type (void) G_GNUC_CONST;
void pt_waveviewer_ruler_set_ruler (PtWaveviewerRuler *self,
                                    gint64 n_samples,
                                    gint px_per_sec,
                                    gint64 duration);
GtkWidget *pt_waveviewer_ruler_new (void);

G_END_DECLS
