pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = parlatype.pc

if GLADE_CATALOG
SUBDIRS = icons
glade_catalogdir   = $(GLADE_CATALOG_DIR)
glade_catalog_DATA = parlatype.xml

validate: $(glade_catalog_DATA)
	xmllint --dtdvalid $(GLADE_CATALOG_DIR)/glade-catalog.dtd --noout $<
endif
