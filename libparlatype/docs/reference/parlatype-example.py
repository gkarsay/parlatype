#!/usr/bin/env python3
# -*- coding: utf-8 -*-

""" parlatype-example.py demonstrates libparlatype.
    Change testfile to an existing file! """

import gi
from gi.repository import Parlatype as Pt
from gi.repository import Gtk
from gi.repository import GObject  # for GObject.timeout
gi.require_version('Gtk', '3.0')

testfile = "file:///home/user/example.mp3"


def error_message(message, parent):
    msg = Gtk.MessageDialog(parent=parent,
                            flags=Gtk.DialogFlags.MODAL,
                            type=Gtk.MessageType.ERROR,
                            buttons=Gtk.ButtonsType.OK,
                            message_format=message)
    msg.run()
    msg.destroy()


""" PtPlayer's error signal.
    It's a severe error and the PtPlayer is reset. There is not much we can
    do about it, here we simply quit. """


def player_error(player, error):
    error_message(error.args[0], win)
    Gtk.main_quit


""" PtPlayer's end-of-stream signal.
    Player is at the end and changes to paused state. Note that stream means
    any playable source like files, too.
    We set our buttons according to the state. """


def player_end_of_stream(player):
    button.set_active(False)


""" The two main functions are Pt.Player.play() and Pt.Player.pause().
    PtWaveviewer scrolls to cursor (follow-cursor) by default but if the user
    has scrolled manually follow-cursor is set to False. Here we assume that on
    play the user wants to follow cursor again. """


def button_toggled(button):
    if button.get_active():
        viewer.set_property("follow-cursor", True)
        player.play()
    else:
        player.pause()


""" In this timeout callback we update cursor position and time label. """


def update_cursor():
    viewer.set_property("playback-cursor", player.get_position())
    text = player.get_current_time_string(Pt.PrecisionType.SECOND_10TH)
    # Sometimes the position can't be retrieved and None is returned.
    if (text):
        label.set_text(text)
    # Continue updating
    return True


""" PtWaveviewer's cursor-changed signal.
    This means the user clicked on the widget to change cursor position.
    We have to inform the PtPlayer. We set the follow-cursor property, too. """


def cursor_changed(viewer, position):
    player.jump_to_position(position)
    viewer.set_property("follow-cursor", True)


""" Player's open async callback.
    Destroy progress dialog and get result. On success get wave data and pass
    it to the PtWaveviewer. Add a timeout of 10 ms to update cursor position
    and time label. """


def open_callback(player, result):
    progress.destroy()
    try:
        player.open_uri_finish(result)
        # Get data at a resolution of 100 px per second
        data = player.get_data(100)
        viewer.set_wave(data)
        GObject.timeout_add(10, update_cursor)
    except Exception as err:
        error_message(err.args[0], win)


""" Pass PtPlayer's emitted progress signal to the PtProgressDialog. """


def progress_update_callback(player, value, dialog):
    dialog.set_progress(value)


""" PtPlayer's async open function can be cancelled.
    This emits an error message for the open callback. """


def progress_response_callback(progress, response):
    if response == Gtk.ResponseType.CANCEL:
        player.cancel()


if testfile == "file:///home/user/example.mp3":
    error_message("Please change 'file:///home/user/example.mp3' in source "
                  "to an existing test file.", None)
    exit()

# PtPlayer has a failable constructor, if GStreamer can't be initted.
try:
    player = Pt.Player.new()
except Exception as err:
    error_message(err.args[0], None)
    exit()

player.connect("error", player_error)
player.connect("end-of-stream", player_end_of_stream)
# For the PtProgressDialog we have to use the async opening function
player.open_uri_async(testfile, open_callback)

# Now create the window with play button, PtWaveviewer and time label.
win = Gtk.Window()
win.set_border_width(12)
win.set_default_size(500, 80)
win.connect("delete-event", Gtk.main_quit)

viewer = Pt.Waveviewer.new()
viewer.connect("cursor-changed", cursor_changed)

button = Gtk.ToggleButton.new_with_label("Play")
button.connect("toggled", button_toggled)

label = Gtk.Label("0:00.0")

hbox = Gtk.Box(spacing=6)
hbox.pack_start(button, False, False, 0)
hbox.pack_start(viewer, True, True, 0)
hbox.pack_start(label, False, False, 0)
win.add(hbox)
win.show_all()

# PtProgressDialog, connect signals and show only, do not progress.run()
progress = Pt.ProgressDialog.new(win)
progress.connect("response", progress_response_callback)
player.connect("load-progress", progress_update_callback, progress)
progress.show_all()

Gtk.main()
