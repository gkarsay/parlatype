# Parlatype

Minimal audio player for manual speech transcription. \
More info and packages available at https://www.parlatype.xyz.

The following instructions are for developers, contributors and those who want to have the latest version from the main branch.

## Build from source

### Dependencies

To build Parlatype from source you need these packages:
* meson >= 0.60.0
* gettext >= 0.19.7
* gobject-introspection-1.0
* yelp-tools
* gtk4 >= 4.14
* glib-2.0 >= 2.76
* libadwaita-1 >= 1.5
* iso-codes
* gstreamer-1.0 >= 1.6.3
* gstreamer-plugins-base-1.0

Optional, depending on your configured options:
* gtk-doc (with `gtk-doc=true`)
* desktop-file-utils (if installed, this checks the desktop file)
* appstream-utils (if installed, this checks the appstream file)
* pocketsphinx >= 5.0 (with `pocketsphinx=true`)
* sphinxbase and pocketsphinx = 5prealpha (with `pocketsphinx-legacy=true`)

Runtime dependencies:
* GStreamer "Good" Plugins

On Debian-based distros install these packages:

```
$ sudo apt-get install meson build-essential libgirepository1.0-dev gtk-doc-tools yelp-tools libgtk-4-dev libgtk-4-1 libadwaita-1-dev iso-codes libgstreamer1.0-dev libgstreamer1.0-0 libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly libsphinxbase-dev libpocketsphinx-dev
```
On Fedora this should work:

```
$ su -c 'dnf install meson gcc gobject-introspection-devel gtk-doc yelp-tools gtk4-devel libadwaita-devel iso-codes-devel gstreamer1-devel gstreamer1-plugins-base-devel gstreamer1-plugins-good gstreamer1-plugins-ugly sphinxbase-devel pocketsphinx-devel'
```

### Configure options

Parlatype ships its own library, _libparlatype_. \
Developers might be interested in having a library documentation and GObject introspection. \
These are the configurable options:

* `gir`: install gobject introspection (default: false)
* `gtk-doc`: install library documentation (default: false)
* `pocketsphinx`: build GStreamer plugin for CMU PocketSphinx support, requires PocketSphinx 5 (default: false)
* `pocketsphinx-legacy`: build GStreamer plugin for CMU PocketSphinx support, requires sphinxbase and PocketSphinx 5prealpha (default: false)

### Build
Clone the repository or download a tarball from https://github.com/gkarsay/parlatype/releases/.

```
$ meson setup build --prefix=/usr
$ cd build
$ ninja
$ sudo ninja install
```
You can use any prefix but you may have to adjust LD_LIBRARY_PATH for other prefixes. \
In this case Meson prints a message with those paths.

## Translate
[Parlatype on Hosted Weblate](https://hosted.weblate.org/engage/parlatype/). \
[![Translation status](https://hosted.weblate.org/widgets/parlatype/-/multi-auto.svg)](https://hosted.weblate.org/engage/parlatype/?utm_source=widget)

You can always add other languages.

## Bugs
Please report bugs at https://github.com/gkarsay/parlatype/issues.
