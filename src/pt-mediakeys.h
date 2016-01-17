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

/* Grabbing media keys from Gnome Settings Daemon.
 * The following is a citation from:
 * https://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/media-keys/README.media-keys-API?h=gnome-3-14

   This is very simple documentation to gnome-settings-daemon's
   D-Bus API for media players.

   gnome-settings-daemon will send key press events from multimedia
   keys to applications that register their interest in those events.
   This allows the play/pause button to control an audio player that's
   not focused for example.

   The D-Bus API is described in gsd-media-keys-manager.c (look for
   introspection_xml), but a small explanation follows here.

   1. Create yourself a proxy object for the remote interface:
   Object path: /org/gnome/SettingsDaemon/MediaKeys
   D-Bus name: org.gnome.SettingsDaemon.MediaKeys
   Interface name: org.gnome.SettingsDaemon.MediaKeys

   2. Register your application with gnome-settings-daemon
   GrabMediaPlayerKeys ("my-application", 0)
   with the second argument being the current time (usually 0,
   or the time passed to you from an event, such as a mouse click)
  
   3. Listen to the MediaPlayerKeyPressed() signal

   4. When receiving a MediaPlayerKeyPressed() signal,
   check whether the first argument (application) matches
   the value you passed to GrabMediaPlayerKeys() and apply the
   action depending on the key (2nd argument)

   Possible values of key are:
   - Play
   - Pause
   - Stop
   - Previous
   - Next
   - Rewind
   - FastForward
   - Repeat
   - Shuffle

   5. Every time your application is focused, you should call
   GrabMediaPlayerKeys() again, so that gnome-settings-daemon knows
   which one was last used. This allows switching between a movie player
   and a music player, for example, and have the buttons control the
   last used application.

   6. When your application wants to stop using the functionality
   it can call ReleaseMediaPlayerKeys(). If your application does
   not call ReleaseMediaPlayerKeys() and releases its D-Bus connection
   then the application will be automatically removed from the list of
   applications held by gnome-settings-daemon.

 */

#ifndef PT_MEDIAKEYS_H
#define PT_MEDIAKEYS_H

#include "config.h"
#include <gtk/gtk.h>
#include "pt-window.h"

void	setup_mediakeys	(PtWindow *win);


#endif
