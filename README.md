# Parlatype

For a screenshot, an overview what Parlatype actually is and packages please visit https://www.parlatype.org.

The following instructions are for developers, contributors and those who want to have the latest version from the main branch.

## Build from source

### Dependencies

To build Parlatype from source you need these packages:
* meson >= 0.47.2 (older versions not tested)
* gettext >= 0.19.7
* gobject-introspection-1.0
* yelp-tools
* gtk+-3.0 >= 3.22
* glib-2.0 >= 2.58
* gnome-desktop-3.0
* gstreamer-1.0 >= 1.6.3
* gstreamer-plugins-base-1.0

Optional, depending on your configure options:
* gladeui-2.0 (>= 3.12.2; with `glade=true`)
* gtk-doc (with `gtk-doc=true`)
* desktop-file-utils (if installed, this checks the desktop file)
* appstream-utils (if installed, this checks the appstream file)
* sphinxbase and pocketsphinx (with `pocketsphinx=true`)
* deepspeech (with `deepspeech=true`)

Runtime dependencies:
* GStreamer "Good" Plugins

Debian based distros have to be Debian >= 10 (Buster) or Ubuntu >= 20.04 (Focal). Install these packages:

```
$ sudo apt-get install meson build-essential libgirepository1.0-dev libgladeui-dev gtk-doc-tools yelp-tools libgtk-3-dev libgtk-3-0 libgnome-desktop-3-dev libgstreamer1.0-dev libgstreamer1.0-0 libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly libatspi2.0-dev libsphinxbase-dev libpocketsphinx-dev
```
On Fedora this should work:

```
$ su -c 'dnf install meson gcc gobject-introspection-devel glade-devel gtk-doc yelp-tools gtk3-devel gnome-desktop-devel gstreamer1-devel gstreamer1-plugins-base-devel gstreamer1-plugins-good gstreamer1-plugins-ugly at-spi2-core-devel sphinxbase-devel pocketsphinx-devel'
```

### Configure options

Parlatype ships its own library, libparlatype. Developers might be interested in having a library documentation, gobject introspection and a glade catalog for the widgets. These are the configure options:

* `gir`: install gobject introspection (default: false)
* `gtk-doc`: install library documentation (default: false)
* `glade`: install a glade catalog (default: false)
* `deepspeech`: build GStreamer plugin for Mozilla DeepSpeech support, requires deepspeech (default: false)
* `pocketsphinx`: build GStreamer plugin for CMU PocketSphinx support, requires sphinxbase and pocketsphinx (default: false)

### Build from git
Clone the repository and build with meson. You can use any prefix but you may have to adjust LD_LIBRARY_PATH for other prefixes. In this case Meson prints a message with those paths.
```
$ git clone https://github.com/gkarsay/parlatype.git
$ cd parlatype
$ meson build --prefix=/usr
$ cd build
$ ninja
$ sudo ninja install
```

### Build from tarball
Download the latest release tarball from https://github.com/gkarsay/parlatype/releases/latest. Assuming it’s version 2.1 and you want the program only:
```
$ wget https://github.com/gkarsay/parlatype/releases/download/v2.1/parlatype-2.1.tar.gz
$ tar -zxvf parlatype-2.1.tar.gz
$ cd parlatype-2.1/
$ meson build --prefix=/usr
$ cd build
$ ninja
$ sudo ninja install
```

## Bugs
Please report bugs at https://github.com/gkarsay/parlatype/issues.
