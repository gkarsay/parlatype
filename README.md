# Parlatype

For a screenshot, an overview what Parlatype actually is and packages please visit https://gkarsay.github.io/parlatype/.

## Installation

### Dependencies

Parlatype switched its build system to Meson. To build it from source, you need these packages:
* meson
* gettext >= 0.19.7
* gobject-introspection-1.0
* yelp-tools
* gtk+-3.0 >= 3.16
* gstreamer-1.0 >= 1.6.3
* gstreamer-plugins-base-1.0
* sphinxbase
* pocketsphinx

Optional, depending on your configure options:
* gladeui-2.0 (>= 3.12.2; with `glade=true`)
* gtk-doc (with `gtk-doc=true`)
* desktop-file-utils (if installed, this checks the desktop file)
* appstream-utils (if installed, this checks the appstream file)

Runtime dependencies:
* GStreamer "Good" Plugins
* If you need MP3 support for GStreamer versions older than 1.14, you have to install the "Ugly" Plugins.
* For the LibreOffice helpers obviously LibreOffice (>= 4) and also libreoffice-script-provider-python (Debian) or libreoffice-pyuno (Fedora)
* You must have the default speech model for pocketsphinx (pocketsphinx-en-us on Debian, pocketsphinx-models on Fedora).

On a Debian based distro you can install all these with:

```
$ sudo apt-get install build-essential libgirepository1.0-dev libgladeui-dev gtk-doc-tools yelp-tools libgtk-3-dev libgtk-3-0 libgstreamer1.0-dev libgstreamer1.0-0 libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly libreoffice-script-provider-python libsphinxbase-dev libpocketsphinx-dev pocketsphinx-en-us
```
On Fedora this should work:

```
$ su -c 'dnf install gcc gobject-introspection-devel glade-devel gtk-doc yelp-tools gtk3-devel gstreamer1-devel gstreamer1-plugins-base-devel gstreamer1-plugins-good gstreamer1-plugins-ugly libreoffice-pyuno sphinxbase-devel pocketsphinx-devel pocketsphinx-models'
```


### Building 

#### Configure options

Parlatype ships its own library, libparlatype. Developers might be interested in having a library documentation, gobject introspection and a glade catalog for the widgets. These are the configure options:

* `libreoffice`: install LibreOffice macros (default: true)
* `libreoffice-dir`: installation folder for LibreOffice macros (default: /usr/lib/libreoffice/share/Scripts/python)
* `gir`: install gobject introspection (default: false)
* `gtk-doc`: install library documentation (default: false)
* `glade`: install a glade catalog (default: false)

#### From git
Clone the repository and build with meson. You can use any prefix but you may have to adjust the library path for other prefixes.
```
$ git clone https://github.com/gkarsay/parlatype.git
$ cd parlatype
$ meson build --prefix=/usr
$ cd build
$ ninja
$ sudo ninja install
```

## Bugs
Please report bugs at https://github.com/gkarsay/parlatype/issues.

