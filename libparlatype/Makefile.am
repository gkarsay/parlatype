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
parlatype_libincludedir = $(libdir)/parlatype-$(PARLATYPE_API_VERSION)/parlatype
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

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all --symbol-prefix=pt --identifier-prefix=Pt #--accept-unprefixed
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = $(libparlatype_@PARLATYPE_API_VERSION@_la_SOURCES) \
			$(parlatype_include_HEADERS)

Parlatype-1.0.gir: libparlatype-1.0.la
Parlatype_1_0_gir_INCLUDES = GObject-2.0 Gtk-3.0
Parlatype_1_0_gir_CFLAGS = \
	-I$(top_srcdir) \
	$(INCLUDES)
Parlatype_1_0_gir_LIBS = libparlatype-1.0.la
Parlatype_1_0_gir_FILES = $(introspection_sources)
#Parlatype_1_0_gir_NAMESPACE = Pt
INTROSPECTION_GIRS += Parlatype-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif
