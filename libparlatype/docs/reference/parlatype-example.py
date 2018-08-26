#!/usr/bin/env python3
# -*- coding: utf-8 -*-

""" parlatype-example.py demonstrates libparlatype.
    Change testfile if needed! """

import os, sys, gi
testfile = "file://" + os.path.abspath("../../tests/data/test1.ogg")

gi.require_version('Gtk', '3.0')
gi.require_version('Parlatype', '1.0')
from gi.repository import Parlatype as Pt, Gtk


def error_message(message, parent):
    msg = Gtk.MessageDialog(parent=parent,
                            flags=Gtk.DialogFlags.MODAL,
                            type=Gtk.MessageType.ERROR,
                            buttons=Gtk.ButtonsType.OK,
                            message_format=message)
    msg.run()
    msg.destroy()

class ParlatypeExample:

    def __init__(self):
        # PtPlayer has a failable constructor, if GStreamer can't be initted.
        try:
            self.player = Pt.Player.new()
        except Exception as err:
            error_message(err.args[0], None)
            exit()

        self.player.connect("error", self.player_error)
        self.player.connect("end-of-stream", self.player_end_of_stream)

        self.builder = Gtk.Builder()
        self.builder.add_from_file('parlatype-example.ui')
        self.builder.connect_signals(self)

        self.win = self.builder.get_object('window')
        self.win.connect("delete-event", Gtk.main_quit)

        self.viewer = self.builder.get_object('waveviewer')
        self.player.connect_waveviewer(self.viewer);

        self.button = self.builder.get_object('button_play')
        self.label = self.builder.get_object('pos_label')

        self.win.show_all()

    def open_callback(self, player, result):
        self.progress.destroy()
        try:
            self.player.open_uri_finish(result)
            # Get data at a resolution of 100 px per second
            data = self.player.get_data(100)
            self.viewer.set_wave(data)
            self.viewer.add_tick_callback(self.update_cursor)
        except Exception as err:
            error_message(err.args[0], self.win)

    def progress_update_callback(self, player, value, dialog):
        dialog.set_progress(value)

    def progress_response_callback(self, progress, response):
        if response == Gtk.ResponseType.CANCEL:
            self.player.cancel()

    def open_testfile(self):
        # For the PtProgressDialog we have to use the async opening function
        self.player.open_uri_async(testfile, self.open_callback)
        # PtProgressDialog, connect signals and show only, do not progress.run()
        self.progress = Pt.ProgressDialog.new(self.win)
        self.progress.connect("response", self.progress_response_callback)
        self.player.connect("load-progress", self.progress_update_callback, self.progress)
        self.progress.show_all()

    def player_error(self, error):
        error_message(error.args[0], self.win)
        Gtk.main_quit

    def player_end_of_stream(self, player):
        self.button.set_active(False)

    def button_toggled(self, button):
        if button.get_active():
            self.viewer.set_property("follow-cursor", True)
            self.player.play()
        else:
            self.player.pause()

    def update_cursor(self, viewer, frame_clock):
        self.viewer.set_property("playback-cursor", self.player.get_position())
        text = self.player.get_current_time_string(Pt.PrecisionType.SECOND_10TH)
        # Sometimes the position can't be retrieved and None is returned.
        if (text):
            self.label.set_text(text)
        # Continue updating
        return True

    def cursor_changed(self, viewer, position):
        self.player.jump_to_position(position)
        viewer.set_property("follow-cursor", True)

def main():
    app = ParlatypeExample()
    app.open_testfile()
    Gtk.main()

if __name__ == "__main__":
    sys.exit(main())
