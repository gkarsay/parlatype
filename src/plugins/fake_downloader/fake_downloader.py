# fake_downloader.py

import gi
import os
import time
from urllib.parse import urlparse

gi.require_version('Pt', '4')

from gi.repository import GObject
from gi.repository import GLib
from gi.repository import Gio
from gi.repository import Pt

class FakeDownloader(GObject.Object, Pt.Downloader):

    config_id: str = GObject.Property(type=str)
    url: str = GObject.Property(type=str)
    sha256: str = GObject.Property(type=str)
    size: int = GObject.Property(type=int)
    current: int = GObject.Property(type=int)
    delay_per_percent: float = GObject.Property(type=float)
    fail_at_percent: int = GObject.Property(type=int)

    dest_folder: str
    dest: str
    cancellable: Gio.Cancellable
    last_progress: float = 0.0

    def progress(self, *args):
        pass

    def progress_cb(self):
        time.sleep(self.delay_per_percent)
        new_current = self.current + self.size / 100
        if new_current > self.size:
            self.current = self.size
        else:
            self.current = new_current;

        return 0

    def download(self, task, other_self, user_data, cancellable):
        while (self.current < self.size):
            if 100 * self.current / self.size >= self.fail_at_percent:
                error = GLib.Error("A fake error occurred", "g-io-error-quark", Gio.IOErrorEnum.FAILED)
                task.return_error(error)
                return
            self.progress_cb()
            if Gio.Cancellable.is_cancelled(self.cancellable):
                task.return_boolean(False)
                return

        task.return_boolean(True)

    def do_download_finish(self, result):
        return result.propagate_boolean()

    def do_download_async(self, cancellable, callback, userdata):
        folder_name = self.config_id
        datadir = GLib.get_user_data_dir()
        file_name = os.path.basename(urlparse(self.url).path)
        self.dest_folder = os.path.join(datadir, 'parlatype', folder_name)
        if not os.path.exists(self.dest_folder):
            os.mkdir(self.dest_folder)
        self.dest = os.path.join(self.dest_folder, file_name)
        open(self.dest,'a')

        self.cancellable = cancellable
        task = Gio.Task.new(self, cancellable, callback, userdata)
        task.run_in_thread(self.download)
