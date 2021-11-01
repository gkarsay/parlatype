#!/usr/bin/env python3
# -*- coding: utf-8 -*-

""" parlatype-example.py demonstrates libparlatype. """

import os
import sys
import gi
gi.require_version('Parlatype', '4.0')
gi.require_version('Gtk', '3.0')
from gi.repository import Parlatype as Pt
from gi.repository import Gtk
from gi.repository import GObject


def error_message(message, parent):
    msg = Gtk.MessageDialog(parent=parent,
                            modal=True,
                            message_type=Gtk.MessageType.ERROR,
                            buttons=Gtk.ButtonsType.OK,
                            text=message)
    msg.run()
    msg.destroy()


class ParlatypeExample:

    def __init__(self):
        self.player = Pt.Player.new()
        self.player.connect("error", self.player_error)
        self.player.connect("end-of-stream", self.player_end_of_stream)

        builder = Gtk.Builder()
        builder.add_from_file('parlatype-example.ui')
        builder.connect_signals(self)

        self.win = builder.get_object('window')
        self.win.connect("delete-event", Gtk.main_quit)

        self.viewer = builder.get_object('waveviewer')
        self.player.connect_waveviewer(self.viewer)

        self.progress = builder.get_object('progress')
        self.viewer.connect('load-progress', self.update_progress)

        self.button = builder.get_object('button_play')
        self.label = builder.get_object('pos_label')
        follow_cursor = builder.get_object('follow_cursor')
        GObject.GObject.bind_property(follow_cursor, "active",
                                      self.viewer, "follow-cursor",
                                      GObject.BindingFlags.BIDIRECTIONAL |
                                      GObject.BindingFlags.SYNC_CREATE)
        volumebutton = builder.get_object('volumebutton')
        GObject.GObject.bind_property(volumebutton, "value",
                                      self.player, "volume",
                                      GObject.BindingFlags.BIDIRECTIONAL |
                                      GObject.BindingFlags.SYNC_CREATE)
        pause = builder.get_object('pause_spin')
        back = builder.get_object('back_spin')
        forward = builder.get_object('forward_spin')
        self.player.set_property("pause", pause.get_value() * 1000)
        self.player.set_property("back", back.get_value() * 1000)
        self.player.set_property("forward", forward.get_value() * 1000)
        self.timer = 0

        self.win.show_all()

    def update_progress(self, viewer, value):
        self.progress.set_fraction(value)

    def open_file(self, filename):
        try:
            self.player.open_uri(filename)
        except Exception as err:
            error_message(err.args[0], self.win)
            if self.timer > 0:
                self.viewer.remove_tick_callback(self.timer)
                self.timer = 0
            return
        # this could also fail, skip checking in this example
        self.viewer.load_wave_async(filename, None, None)
        self.timer = self.viewer.add_tick_callback(self.update_cursor)

    def filechooser(self, button):
        dialog = Gtk.FileChooserDialog(
            title="Please choose a file",
            parent=self.win,
            action=Gtk.FileChooserAction.OPEN)
        dialog.add_button("_Cancel", Gtk.ResponseType.CANCEL)
        dialog.add_button("_Open", Gtk.ResponseType.OK)
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.open_file(dialog.get_uri())
        dialog.destroy()

    def player_error(self, player, error):
        error_message(error.args[0], self.win)
        Gtk.main_quit

    def player_end_of_stream(self, player):
        GObject.GObject.handler_block_by_func(self.button, self.button_toggled)
        self.button.set_active(False)
        GObject.GObject.handler_unblock_by_func(self.button, self.button_toggled)
        self.player.pause()

    def button_toggled(self, button):
        if button.get_active():
            self.viewer.set_property("follow-cursor", True)
            self.player.play()
        else:
            self.player.pause_and_rewind()

    def update_cursor(self, viewer, frame_clock):
        viewer.set_property("playback-cursor", self.player.get_position())
        text = self.player.get_current_time_string(
            Pt.PrecisionType.SECOND_10TH)
        # Sometimes the position can't be retrieved and None is returned.
        if (text):
            self.label.set_text(text)
        # Continue updating
        return True

    def show_ruler_toggled(self, button):
        self.viewer.set_property("show-ruler", button.get_active())

    def fixed_cursor_toggled(self, button):
        self.viewer.set_property("fixed-cursor", button.get_active())

    def speed_value_changed(self, gtkrange):
        self.player.set_speed(gtkrange.get_value())

    def pause_value_changed(self, gtkrange):
        self.player.set_property("pause", gtkrange.get_value() * 1000)

    def back_value_changed(self, gtkrange):
        self.player.set_property("back", gtkrange.get_value() * 1000)

    def forward_value_changed(self, gtkrange):
        self.player.set_property("forward", gtkrange.get_value() * 1000)

    def back_clicked(self, button):
        self.player.jump_back()

    def forward_clicked(self, button):
        self.player.jump_forward()


def main():
    app = ParlatypeExample()
    Gtk.main()


if __name__ == "__main__":
    sys.exit(main())
