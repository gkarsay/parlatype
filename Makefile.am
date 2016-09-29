SUBDIRS = data help libparlatype docs libreoffice po src # docs must be after libparlatype

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = --enable-introspection

EXTRA_DIST = \
	config.rpath	\
	COPYING_CCBYSA3	\
	README.md

DISTCLEANFILES = `find . -mindepth 1 -iname "*~"`

