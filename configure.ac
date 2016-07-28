#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([parlatype], [1.0], [gabor.karsay@gmx.at])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST([GETTEXT_PACKAGE], AC_PACKAGE_NAME)
IT_PROG_INTLTOOL(0.50.2)
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([setlocale])

GLIB_GSETTINGS                 
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

YELP_HELP_INIT

GTK_MIN_VERSION=3.10.0
PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= $GTK_MIN_VERSION, gmodule-2.0, gstreamer-1.0])
PACKAGE_CFLAGS="-std=c99 $GTK_CFLAGS"
PACKAGE_LIBS="$GTK_LIBS"
PACKAGE_LDFLAGS="$GTK_LDFLAGS"

AC_SUBST([PACKAGE_CFLAGS])
AC_SUBST([PACKAGE_LIBS])
AC_SUBST([PACKAGE_LDFLAGS])

AC_ARG_WITH([lo],
            [AS_HELP_STRING([--without-lo],
              [don't install LibreOffice macros])],
            [],
            [with_lo=yes])

AM_CONDITIONAL([LO], [test "$with_lo" = "yes"])

AC_CONFIG_FILES([Makefile
                 data/Makefile
		 data/parlatype.desktop.in
		 data/icons/Makefile
		 docs/Makefile
		 help/Makefile
		 libreoffice/Makefile
		 po/Makefile.in
                 src/Makefile])
AC_OUTPUT

dnl ==========================================================================
echo "

        Parlatype $VERSION
        ==========================

        prefix ....: ${prefix}
	with-lo ...: $with_lo"

if test "${prefix}" != "/usr" -a "$with_lo" == "yes"; then
echo "
        Note: Consider using --without-lo for prefixes
        other than /usr and copying the script manually to:
        /usr/lib/libreoffice/share/Scripts/python/Parlatype.py"
fi
echo "
        Now type 'make' to build $PACKAGE
"
