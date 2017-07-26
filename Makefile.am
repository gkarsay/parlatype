SUBDIRS = libparlatype data docs help libreoffice po src

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config.rpath	\
	COPYING_CCBYSA	\
	README.md

CLEANFILES = \
	libparlatype/docs/reference/*.bak

DISTCLEANFILES = \
	`find . -mindepth 1 -iname "*~"`

# Distcheck flags for libparlatype:
# without glade catalog, because glade catalog is installed into a detected dir
# that we have no permissions for.
DISTCHECK_CONFIGURE_FLAGS = \
	--enable-introspection	\
	--enable-gtk-doc	\
	--disable-glade-catalog


# A few maintainer make targets

.PHONY: dist-sign compat-check help help-online pot todo lp-export lp-create-tar
LP_EXPORT_DIR = tmp
LP_EXPORT_FILE = launchpad-upload.tar.gz

# make compat-check  -- list all GTK/Glib/GStreamer version checks
compat-check:
	@find . \( -name "*.c" -o -name "*.h" \) -exec grep -Hno "\(GLIB\|GTK\|GST\)_CHECK_VERSION.*" {} \;

# make dist-sign     -- sign tarball
dist-sign:
	gpg --armor --detach-sign "parlatype-$(VERSION).tar.gz"

# make help-online   -- create html version of help
help-online:
	mkdir -p help-online
	yelp-build html -o help-online/ help/C/

# make lp-export     -- create archive to export translations to launchpad
lp-export: $(LP_EXPORT_FILE)

$(LP_EXPORT_FILE): pot lp-create-tar

lp-create-tar:
	mkdir $(LP_EXPORT_DIR)
	mkdir $(LP_EXPORT_DIR)/libparlatype
	mkdir $(LP_EXPORT_DIR)/parlatype
	mkdir $(LP_EXPORT_DIR)/help
	cp libparlatype/po/*.pot $(LP_EXPORT_DIR)/libparlatype
	cp libparlatype/po/*.po $(LP_EXPORT_DIR)/libparlatype
	cp po/*.pot $(LP_EXPORT_DIR)/parlatype
	cp po/*.po $(LP_EXPORT_DIR)/parlatype
	cp help/*.pot $(LP_EXPORT_DIR)/help/help.pot
	find help/ -iname "*.po" -exec cp {} $(LP_EXPORT_DIR)/help \;
	tar -C $(LP_EXPORT_DIR) -czf $(LP_EXPORT_FILE) libparlatype parlatype help
	rm -r $(LP_EXPORT_DIR)/

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


maintainer-clean-local:
	rm -rf $(LP_EXPORT_DIR)
	rm -f $(LP_EXPORT_FILE)

