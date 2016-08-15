/* Buzztrax
 * Copyright (C) 2008 Buzztrax team <buzztrax-devel@buzztrax.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BT_WAVEFORM_VIEWER_H
#define BT_WAVEFORM_VIEWER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BT_TYPE_WAVEFORM_VIEWER          (bt_waveform_viewer_get_type ())
#define BT_WAVEFORM_VIEWER(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), BT_TYPE_WAVEFORM_VIEWER, BtWaveformViewer))
#define BT_IS_WAVEFORM_VIEWER(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BT_TYPE_WAVEFORM_VIEWER))
#define BT_WAVEFORM_VIEWER_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  BT_TYPE_WAVEFORM_VIEWER, BtWaveformViewerClass))
#define BT_IS_WAVEFORM_VIEWER_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  BT_TYPE_WAVEFORM_VIEWER))

typedef struct _BtWaveformViewer      BtWaveformViewer;
typedef struct _BtWaveformViewerClass BtWaveformViewerClass;

/**
 * BtWaveformViewer:
 *
 * waveform view widget
 */
struct _BtWaveformViewer {
  GtkWidget parent;

  gfloat *peaks;
  gint peaks_size;
  gint channels;
    
  gint64 wave_length;
  gint64 loop_start, loop_end;
  gint64 playback_cursor;
  
  /* state */
  GdkWindow *window;
  GtkBorder border;
  gboolean edit_loop_start, edit_loop_end, edit_selection;
};

struct _BtWaveformViewerClass {
  GtkWidgetClass klass;
};

GtkWidget *bt_waveform_viewer_new(void);

void bt_waveform_viewer_set_wave(BtWaveformViewer *self, gint16 *data, gint channels, gint length);

GType bt_waveform_viewer_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif // BT_WAVEFORM_VIEWER_H
