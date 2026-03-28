/* Copyright 2025 Gabor Karsay <gabor.karsay@gmx.at>
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

/**
 * PtVolumeButton:
 *
 * Controls volume of playback.
 */

#include "config.h"

#include "pt-volume-button.h"

struct _PtVolumeButton
{
  AdwBin     parent;
  GtkWidget *button;
  GStrv      vol_icons;
  PtPlayer  *player;
};

G_DEFINE_FINAL_TYPE (PtVolumeButton, pt_volume_button, ADW_TYPE_BIN)

static void
button_update_mute (GObject    *gobject,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  /* Switch icons of volume button depending on mute state.
   * If muted, only the mute icon is shown (volume can still be adjusted).
   * In normal state restore original icon set. */

  PtVolumeButton *self = PT_VOLUME_BUTTON (user_data);
  GtkScaleButton *volumebutton = GTK_SCALE_BUTTON (self->button);
  const gchar    *mute_icon[] = { (gchar *) self->vol_icons[0], NULL };

  if (pt_player_get_mute (self->player))
    {
      gtk_scale_button_set_icons (volumebutton, mute_icon);
    }
  else
    {
      gtk_scale_button_set_icons (volumebutton, (const gchar **) self->vol_icons);
    }
}

static void
button_update_volume (GObject    *gobject,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  PtVolumeButton *self = PT_VOLUME_BUTTON (user_data);
  GtkScaleButton *volumebutton = GTK_SCALE_BUTTON (self->button);
  gdouble         volume = pt_player_get_volume (self->player);

  gtk_scale_button_set_value (volumebutton, volume);
}

static gboolean
button_value_changed_cb (GtkWidget *volumebutton,
                         gdouble    value,
                         gpointer   user_data)
{
  PtVolumeButton *self = PT_VOLUME_BUTTON (user_data);
  pt_player_set_volume (self->player, value);
  return FALSE;
}

static gboolean
button_event_cb (GtkEventControllerLegacy *ctrl,
                 GdkEvent                 *event,
                 gpointer                  user_data)
{
  /* This is for pulseaudiosink in paused state. It doesn't notify of
   * volume/mute changes. Update values when user is interacting with
   * the volume button. */

  PtVolumeButton *self = PT_VOLUME_BUTTON (user_data);
  GtkWidget      *volumebutton;

  volumebutton = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (ctrl));
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (volumebutton),
                              pt_player_get_volume (self->player));
  button_update_mute (NULL, NULL, self);
  return FALSE;
}

static gboolean
button_press_event (GtkGestureClick *gesture,
                    int              n_press,
                    gdouble          x,
                    gdouble          y,
                    gpointer         user_data)
{
  /* Switch mute state on click with secondary button */

  PtVolumeButton *self = PT_VOLUME_BUTTON (user_data);
  guint           button;
  gboolean        current_mute;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (n_press == 1 && button == GDK_BUTTON_SECONDARY)
    {
      current_mute = pt_player_get_mute (self->player);
      pt_player_set_mute (self->player, !current_mute);
      return TRUE;
    }
  return FALSE;
}

/**
 * pt_volume_button_set_player:
 * @self: a `PtVolumeButton`
 * @player: a `PtPlayer`
 *
 * Set the player that will be controlled by the button
 * and that emits signals to the button.
 */
void
pt_volume_button_set_player (PtVolumeButton *self,
                             PtPlayer       *player)
{
  self->player = player;

  g_signal_connect (self->player,
                    "notify::mute",
                    G_CALLBACK (button_update_mute),
                    self);

  g_signal_connect (self->player,
                    "notify::volume",
                    G_CALLBACK (button_update_volume),
                    self);
}

static void
pt_volume_button_dispose (GObject *object)
{
  PtVolumeButton *self = PT_VOLUME_BUTTON (object);

  if (self->vol_icons)
    {
      g_strfreev (self->vol_icons);
      self->vol_icons = NULL;
    }

  gtk_widget_dispose_template (GTK_WIDGET (self), PT_TYPE_VOLUME_BUTTON);

  // G_OBJECT_CLASS (pt_volume_button_parent_class)->dispose (object);
}

static void
pt_volume_button_init (PtVolumeButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Connect to volumebutton's "value-changed" signal and to PtPlayer's
   * "notify:volume" signal instead of binding the 2 properties. On
   * startup there is no external Pulseaudio volume yet and the binding
   * would impose our default volume on Pulseaudio. Instead we want to
   * pick up Pulseaudio's volume. That is Parlatype/application specific
   * and it even remembers the last value. The first external volume
   * signal is sent after a file was opened successfully, until then our
   * volume button is insensitive. */
  g_signal_connect (self->button,
                    "value-changed",
                    G_CALLBACK (button_value_changed_cb),
                    self);

  GtkEventController *v_event;
  v_event = gtk_event_controller_legacy_new ();
  g_signal_connect (v_event,
                    "event",
                    G_CALLBACK (button_event_cb),
                    self);
  gtk_widget_add_controller (self->button,
                             GTK_EVENT_CONTROLLER (v_event));

  /* Get complete icon set of volume button to change it depending on
   * mute state. */
  g_object_get (self->button,
                "icons", &self->vol_icons, NULL);

  /* Switch mute state on mouse click with secondary button */
  GtkGesture *vol_event;
  vol_event = gtk_gesture_click_new ();
  gtk_gesture_single_set_exclusive (
      GTK_GESTURE_SINGLE (vol_event), TRUE);
  gtk_gesture_single_set_button (
      GTK_GESTURE_SINGLE (vol_event), 0);
  gtk_event_controller_set_propagation_phase (
      GTK_EVENT_CONTROLLER (vol_event), GTK_PHASE_CAPTURE);
  g_signal_connect (vol_event,
                    "pressed",
                    G_CALLBACK (button_press_event),
                    self);
  gtk_widget_add_controller (self->button,
                             GTK_EVENT_CONTROLLER (vol_event));
}

static void
pt_volume_button_class_init (PtVolumeButtonClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pt_volume_button_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/xyz/parlatype/Parlatype/volume-button.ui");
  gtk_widget_class_bind_template_child (widget_class, PtVolumeButton, button);
}

/**
 * pt_volume_button_new:
 *
 * Constructor for `PtVolumeButton`.
 *
 * Returns: a newly created `PtVolumeButton`.
 */
PtVolumeButton *
pt_volume_button_new (void)
{
  return g_object_new (PT_TYPE_VOLUME_BUTTON, NULL);
}
