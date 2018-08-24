# Parlatype

For a screenshot, an overview what Parlatype actually is and packages please visit https://gkarsay.github.io/parlatype/.

## Installation

### Dependencies

#### Stable version 1.5.6

To build Parlatype from source you need these packages:
* make, autotools (autoconf, automake)
* intltool
* gobject-introspection-1.0
* yelp-tools
* gtk+-3.0 (>= 3.10)
* gstreamer-1.0
* gstreamer-plugins-base-1.0

Optional, depending on your configure options:
* gladeui-2.0 (>= 3.12.2; with `--enable-glade-catalog`)
* gtk-doc-tools (with `--enable-gtk-docs`)
* desktop-file-utils (if installed, this checks the desktop file)
* appstream-utils (if installed, this checks the appstream file)

Required runtime dependencies:
* GTK+ 3
* GStreamer
* GStreamer "Good" Plugins

Optional runtime dependencies, to support MP3 files:
* GStreamer "Ugly" Plugins

Optional runtime dependencies, if you want to use LibreOffice macros:
* LibreOffice (>= 4)
* libreoffice-script-provider-python (Debian only)

On a Debian based distro you can install all these with:

```
$ sudo apt-get install build-essential automake autoconf intltool libgirepository1.0-dev libgladeui-dev gtk-doc-tools yelp-tools libgtk-3-dev libgtk-3-0 libgstreamer1.0-dev libgstreamer1.0-0 libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly libreoffice-script-provider-python
```

#### Master

There are a few changes in this cycle. If you build from git master the changes compared to version 1.5.6 are:
* intltool: this dependency is dropped
* gettext >= 0.19.7
* GTK+ >= 3.16
* GStreamer >= 1.6.3

### Building 

#### Configure options

Parlatype ships its own library, libparlatype. Developers might be interested in having a library documentation, gobject introspection and a glade catalog for the widgets. These are the configure options:

* `--with-libreoffice`: install LibreOffice macros (default: yes)
* `--enable-introspection`: install gobject introspection (default: yes)
* `--enable-tests`: build unit tests (default: yes) – please note that these tests need a runtime environment and will fail in a pure build environment
* `--enable-gtk-doc`: install library documentation (default: no)
* `--enable-glade-catalog`: install a glade catalog (default: no)
* `--enable-code-coverage`: enable gcov/lcov code coverage (default: no)

If you want the program only, you would use `--prefix=/usr --disable-introspection`.

#### From git
Clone the repository and run autogen.sh. Assuming you want the program only:
```
$ git clone https://github.com/gkarsay/parlatype.git
$ cd parlatype
$ ./autogen.sh --prefix=/usr --disable-introspection
$ make
$ sudo make install
```

#### From tarball
Download the latest release tarball from https://github.com/gkarsay/parlatype/releases/latest. Assuming it's version 1.5.6 and you want the program only:
```
$ wget https://github.com/gkarsay/parlatype/releases/download/v1.5.6/parlatype-1.5.6.tar.gz
$ tar -zxvf parlatype-1.5.6.tar.gz
$ cd parlatype-1.5.6/
$ autoreconf # might be necessary
$ ./configure --prefix=/usr --disable-introspection
$ make
$ sudo make install
```

### LibreOffice helpers
The LibreOffice helpers/macros are installed together with Parlatype. However, this works only fine with `--prefix=/usr`. If you use a different prefix, it’s recommended to
```
$ ./configure --without-libreoffice
```
In this case, please copy the macros manually:
```
$ sudo cp libreoffice/Parlatype.py /usr/lib/libreoffice/share/Scripts/python/
```
If you don’t want to install them system-wide, you can put them in your home dir instead:
```
$ cp libreoffice/Parlatype.py ~/.config/libreoffice/4/user/Scripts/python/
```
There is an integrated help in Parlatype with a page describing how to use the LibreOffice helpers.

## Bugs
Please report bugs at https://github.com/gkarsay/parlatype/issues.

