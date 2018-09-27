SUBDIRS = data po src docs # docs must be after src

if WITH_TESTS
SUBDIRS += tests
endif

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# Distribute test infrastructure
# ax_code_coverage.m4 and ax_valgrind_check.m4 would be available in autoconf-archive
# ax_code_coverage.m4 first in version 2014.10.15 but we support older systems
# ax_valgrind_check.m4 first in version 2015.02.04 but we support older systems

EXTRA_DIST = \
	config.rpath		\
	glib-tap.mk		\
	meson.build		\
	tap-driver.sh		\
	tap-test		\
	test-driver		\
	m4/ax_code_coverage.m4	\
	m4/ax_valgrind_check.m4	\
	m4/glibtests.m4		\
	po/meson.build

DISTCLEANFILES = `find . -mindepth 1 -iname "*~"`

