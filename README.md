# Parlatype

For a screenshot, an overview what Parlatype actually is and packages please visit http://gkarsay.github.io/parlatype/.

## Installation

### Dependencies

To install Parlatype from source you need these packages: make, autotools, intltool, gobject-introspection-1.0, gladeui-2.0, gtk-doc-tools, yelp-tools and finally gtk+-3.0 (minimum version 3.10) and gstreamer-1.0 with base plugins.

On a Debian based distro you can install these with:

```
$ sudo apt-get install build-essential automake autoconf intltool libgirepository1.0-dev libgladeui-dev gtk-doc-tools yelp-tools libgtk-3-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

In order to run it needs at least Gtk+ 3.10 and GStreamer 1.0 with the set of “good” plugins.
The Debian based command is (but you probably have them already):
```
$ sudo apt-get install libgtk-3-0 libgstreamer1.0-0 gstreamer1.0-plugins-good
```

### Building 

#### Configure options

Parlatype ships its own library, libparlatype. Developers might be interested in having a library documentation, gobject introspection and a glade catalog for the widgets. These are the configure options:

* `--enable-gtk-doc`: install library documentation (default: no)
* `--enable-introspection`: install gobject introspection (default: yes)
* `--enable-glade-catalog`: install a glade catalog (default: no)
* `--with-libreoffice`: install LibreOffice macros (default: yes)

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
Download the latest release tarball from https://github.com/gkarsay/parlatype/releases/latest. Assuming it's version 1.5 and you want the program only:
```
$ wget https://github.com/gkarsay/parlatype/releases/download/v1.5/parlatype-1.5.tar.gz
$ tar -zxvf parlatype-1.5.tar.gz
$ cd parlatype-1.5/
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

