SUBDIRS = libparlatype data docs help libreoffice po src

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config.rpath	\
	COPYING_CCBYSA	\
	README.md

DISTCLEANFILES = `find . -mindepth 1 -iname "*~"`

# `make pot` to create all translation templates at once
.PHONY: pot
pot:
	$(MAKE) -C po parlatype.pot
	$(MAKE) -C help pot
	$(MAKE) -C libparlatype/po libparlatype.pot
