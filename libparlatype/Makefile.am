SUBDIRS = data po src docs # docs must be after src

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = --enable-introspection

EXTRA_DIST = \
	config.rpath

DISTCLEANFILES = `find . -mindepth 1 -iname "*~"`

