# whisper_downloader.py

import gi
import pycurl
import hashlib
import os
from urllib.parse import urlparse

gi.require_version('Pt', '4')

from gi.repository import GObject
from gi.repository import GLib
from gi.repository import Gio
from gi.repository import Pt

class WhisperDownloader(GObject.Object, Pt.Downloader):

    config_id: str = GObject.Property(type=str)
    url: str = GObject.Property(type=str)
    sha256: str = GObject.Property(type=str)
    size: int = GObject.Property(type=int)
    current: int = GObject.Property(type=int)

    dest_folder: str
    dest: str
    cancellable: Gio.Cancellable
    last_progress: float = 0.0

    def progress(self, *args):
        pass

    def progress_cb(self, download_total, download_done, upload_t, upload_d):
        if Gio.Cancellable.is_cancelled(self.cancellable):
            return 1

        if download_done - self.current > 1000:
            self.current = download_done;

        return 0

    def download(self, task, other_self, user_data, cancellable):
        '''
        print("self: ", self)
        print("task: ", task)
        print("other_self: ", other_self)
        print("data: ", user_data)
        print("cancellable: ", cancellable)
        '''

        # http://pycurl.io/docs/latest/quickstart.html
        try:
            with open(self.dest, 'wb') as file:
                c = pycurl.Curl()
                c.setopt(c.URL, self.url)
                c.setopt(c.FOLLOWLOCATION, True)
                c.setopt(c.NOPROGRESS, False)
                c.setopt(c.XFERINFOFUNCTION, self.progress_cb)
                c.setopt(c.WRITEDATA, file)
                c.perform()
                c.close()
        except pycurl.error as e:
            error = GLib.Error(e.args[1], "g-io-error-quark", Gio.IOErrorEnum.FAILED)
            task.return_error(error)
            return

        if not self.checksum():
            error = GLib.Error("Checksum doesn't match", "g-io-error-quark", Gio.IOErrorEnum.FAILED)
            task.return_error(error)
            return

        task.return_boolean(True)

    def do_download_finish(self, result):
        return result.propagate_boolean()

    def do_download_async(self, cancellable, callback, userdata):
        '''
        print("download_async: self: ", self)
        print("download_async: cancellable: ", cancellable)
        print("download_async: callback: ", callback)
        print("download_async: userdata: ", userdata)
        '''
        folder_name = self.config_id
        datadir = GLib.get_user_data_dir()
        file_name = os.path.basename(urlparse(self.url).path)
        self.dest_folder = os.path.join(datadir, 'parlatype', folder_name)
        if not os.path.exists(self.dest_folder):
            os.mkdir(self.dest_folder)
        self.dest = os.path.join(self.dest_folder, file_name)

        '''
        print("downloading " + self.url)
        print("saving to " + self.dest)
        print("checksum " + self.sha256)
        print("size {}".format(self.size))
        '''

        self.cancellable = cancellable
        task = Gio.Task.new(self, cancellable, callback)
        task.run_in_thread(self.download)

    def checksum(self):
        print("checking sum...")
        h = hashlib.sha256()
        with open(self.dest, 'rb') as file:
            while True:
                data = file.read(4096)
                if len(data) == 0:
                    break
                else:
                    h.update(data)
        return self.sha256 == h.hexdigest()
