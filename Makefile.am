SUBDIRS = data docs po src

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config.rpath	\
	COPYING_CCBYSA3	\
	org.gnome.parlatype.gschema.xml

gsettings_SCHEMAS = org.gnome.parlatype.gschema.xml
@GSETTINGS_RULES@

DISTCLEANFILES = `find . -mindepth 1 -iname "*~"`

