lib_LTLIBRARIES = libparlatype.la

resource_xml = $(srcdir)/resources/libparlatype.gresource.xml
resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir)/resources --generate-dependencies $(resource_xml))
pt-resources.c: $(resource_xml) $(resource_files)
	$(AM_V_GEN)glib-compile-resources	\
		--target=$@			\
		--sourcedir=$(srcdir)/resources	\
		--generate-source		\
		--c-name parlatype $(resource_xml)
pt-resources.h: $(resource_xml) $(resource_files)
	$(AM_V_GEN)glib-compile-resources 	\
		--target=$@			\
		--sourcedir=$(srcdir)/resources	\
		--generate-header		\
		--c-name parlatype $(resource_xml)

BUILT_SOURCES =		\
	pt-resources.c	\
	pt-resources.h	\
	$(NULL)

libparlatype_la_SOURCES =	\
	$(BUILT_SOURCES)	\
	pt-player.c		\
	pt-progress-dialog.c	\
	pt-ruler.c		\
	pt-wavedata.c		\
	pt-waveloader.c		\
	pt-waveviewer.c		\
	pt-waveviewer-focus.c

parlatype_includedir= \
	$(includedir)/parlatype

parlatype_include_HEADERS =	\
	pt-player.h		\
	pt-progress-dialog.h	\
	pt-wavedata.h		\
	pt-waveloader.h		\
	pt-waveviewer.h

AM_CPPFLAGS =			\
	$(CODE_COVERAGE_CFLAGS)	\
	$(WARNINGCFLAGS)	\
	$(GTK_CFLAGS)		\
	$(GSTREAMER_CFLAGS)

libparlatype_la_LDFLAGS =			\
	-version-info $(PARLATYPE_SO_VERSION)	\
	-export-symbols-regex '^pt_'		\
	-export-dynamic

libparlatype_la_LIBADD =	\
	$(CODE_COVERAGE_LIBS)	\
	$(GTK_LIBS)		\
	$(GSTREAMER_LIBS)

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
parlatype_libincludedir = \
	 $(libdir)/parlatype
#nodist_parlatype_libinclude_HEADERS = xxx.h


-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all --symbol-prefix=pt --identifier-prefix=Pt #--accept-unprefixed
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION

# introspection sources without built sources, so not using
# $(libparlatype_la_SOURCES)
introspection_sources =			\
		pt-player.c		\
		pt-progress-dialog.c	\
		pt-wavedata.c		\
		pt-waveloader.c		\
		pt-waveviewer.c		\
		$(parlatype_include_HEADERS)

Parlatype-1.0.gir: libparlatype.la
Parlatype_1_0_gir_INCLUDES = GObject-2.0 Gtk-3.0
Parlatype_1_0_gir_CFLAGS = \
	-I$(top_srcdir) \
	$(INCLUDES)
Parlatype_1_0_gir_LIBS = libparlatype.la
Parlatype_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Parlatype-1.0.gir

girdir   = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir   = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA) $(BUILT_SOURCES)
endif

EXTRA_DIST = 			\
	$(resource_files)	\
	$(resource_xml)
