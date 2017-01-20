SUBDIRS = libparlatype data docs help libreoffice po src

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config.rpath	\
	COPYING_CCBYSA	\
	README.md

CLEANFILES = libparlatype/docs/reference/*.bak
DISTCLEANFILES = `find . -mindepth 1 -iname "*~"`

# Distcheck flags for libparlatype:
# without glade catalog, because glade catalog is installed into a detected dir
# that we have no permissions for.
DISTCHECK_CONFIGURE_FLAGS = \
	--enable-introspection	\
	--enable-gtk-doc	\
	--disable-glade-catalog


# A few maintainer make targets

.PHONY: gtk-check help pot todo
# make gtk-check     -- list all gtk version checks
gtk-check:
	@find . \( -name "*.c" -o -name "*.h" \) -exec grep -Hno "GTK_CHECK_VERSION.*" {} \;

# make pot           -- create all translation templates at once
pot:
	$(MAKE) -C po parlatype.pot
	$(MAKE) -C help pot
	$(MAKE) -C libparlatype/po libparlatype.pot

# make todo          -- list all FIXME and TODO items
todo:
	@find . \( -name "*.c" -o -name "*.h" \) -exec grep -Hno "FIXME .*" {} \; | sed "s/\*\///"
	@find . \( -name "*.c" -o -name "*.h" \) -exec grep -Hno "TODO .*" {} \; | sed "s/\*\///"

# make help          -- show this list
help:
	@grep -e "^# make .*" Makefile | cut -c3-
