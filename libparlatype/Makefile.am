lib_LTLIBRARIES = libparlatype-@PARLATYPE_API_VERSION@.la
libparlatype_@PARLATYPE_API_VERSION@_la_SOURCES = \
		pt-player.c	\
		pt-waveloader.c	\
		pt-waveslider.c

parlatype_includedir=$(includedir)/parlatype-$(PARLATYPE_API_VERSION)
parlatype_include_HEADERS = \
		pt-player.h	\
		pt-waveloader.h	\
		pt-waveslider.h

AM_CPPFLAGS = $(PACKAGE_CFLAGS)		\
	      -DUIDIR=\"$(pkgdatadir)\" \
	      -DLOCALEDIR=\"$(localedir)\"

libparlatype_@PARLATYPE_API_VERSION@_la_LDFLAGS = -version-info $(PARLATYPE_SO_VERSION)
libparlatype_@PARLATYPE_API_VERSION@_la_LIBADD = $(PACKAGE_LIBS)

## The generated configuration header is installed in its own subdirectory of
## $(libdir).  The reason for this is that the configuration information put
## into this header file describes the target platform the installed library
## has been built for.  Thus the file must not be installed into a location
## intended for architecture-independent files, as defined by the Filesystem
## Hierarchy Standard (FHS).
## The nodist_ prefix instructs Automake to not generate rules for including
## the listed files in the distribution on 'make dist'.  Files that are listed
## in _HEADERS variables are normally included in the distribution, but the
## configuration header file is generated at configure time and should not be
## shipped with the source tarball.
parlatype_libincludedir = $(libdir)/parlatype-$(PARLATYPE_API_VERSION)/include
#nodist_parlatype_libinclude_HEADERS = xxx.h

## Install the generated pkg-config file (.pc) into the expected location for
## architecture-dependent package configuration information.  Occasionally,
## pkg-config files are also used for architecture-independent data packages,
## in which case the correct install location would be $(datadir)/pkgconfig.
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libparlatype.pc

## Define an independent executable script for inclusion in the distribution
## archive.  However, it will not be installed on an end user's system due to
## the noinst_ prefix.
#dist_noinst_SCRIPTS = autogen.sh

