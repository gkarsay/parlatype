SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = parlatype.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

install-desktop-database: install-desktopDATA
	update-desktop-database $(DESTDIR)$(desktopdir)

gsettings_SCHEMAS = org.gnome.parlatype.gschema.xml
@GSETTINGS_RULES@

CLEANFILES = \
	$(desktop_in_files)	\
	$(desktop_DATA)

EXTRA_DIST = \
	org.gnome.parlatype.gschema.xml	\
	parlatype.desktop.in.in
