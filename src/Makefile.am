bin_PROGRAMS=parlatype

resource_xml = $(srcdir)/resources/parlatype.gresource.xml
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

BUILT_SOURCES = \
	pt-resources.c		\
	pt-resources.h		\
	$(NULL)

parlatype_SOURCES = \
		$(BUILT_SOURCES) \
		main.c		\
		pt-app.c	\
		pt-app.h	\
		pt-dbus-service.c	\
		pt-dbus-service.h	\
		pt-goto-dialog.c	\
		pt-goto-dialog.h	\
		pt-mediakeys.c	\
		pt-mediakeys.h	\
		pt-preferences.c	\
		pt-preferences.h	\
		pt-progress-dialog.c	\
		pt-progress-dialog.h	\
		pt-window.c	\
		pt-window.h	\
		pt-window-dnd.c	\
		pt-window-dnd.h	\
		pt-window-private.h

parlatype_LDFLAGS = $(PACKAGE_LDFLAGS)

parlatype_LDADD = 	\
	$(top_builddir)/libparlatype/libparlatype-1.0.la	\
	$(PACKAGE_LIBS)

AM_CPPFLAGS = $(PACKAGE_CFLAGS)		\
	      -DUIDIR=\"$(pkgdatadir)\" \
	      -DLOCALEDIR=\"$(localedir)\"

EXTRA_DIST = \
	$(resource_files) \
	$(resource_xml)

CLEANFILES = $(BUILT_SOURCES)
