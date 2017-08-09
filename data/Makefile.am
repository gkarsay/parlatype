SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = com.github.gkarsay.parlatype.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

install-desktop-database: install-desktopDATA
	update-desktop-database $(DESTDIR)$(desktopdir)

gsettings_SCHEMAS = com.github.gkarsay.parlatype.gschema.xml
@GSETTINGS_RULES@

appdatadir = $(datadir)/metainfo
appdata_in_files = com.github.gkarsay.parlatype.appdata.xml.in
if LIBREOFFICE
appdata_in_files += com.github.gkarsay.parlatype.libreoffice-helpers.metainfo.xml.in
endif
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

dbusservicedir = $(datadir)/dbus-1/services
dbusservice_DATA = com.github.gkarsay.parlatype.service

com.github.gkarsay.parlatype.service: Makefile
	$(AM_V_GEN) (echo '[D-BUS Service]'; \
		echo 'Name=com.github.gkarsay.parlatype'; \
		echo 'Exec=${bindir}/parlatype --gapplication-service') > $@.tmp && \
		mv $@.tmp $@

check-local: $(desktop_DATA) $(appdata_DATA)
	$(DESKTOP_FILE_VALIDATE) $(desktop_DATA)
	$(APPDATA_VALIDATE) validate-relax --nonet $(appdata_DATA)

CLEANFILES = \
	$(appdata_DATA)		\
	$(dbusservice_DATA)	\
	$(desktop_DATA)

EXTRA_DIST = \
	com.github.gkarsay.parlatype.gschema.xml	\
	$(desktop_in_files)		\
	$(appdata_in_files)
