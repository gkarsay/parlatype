#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([parlatype], [1.0], [gabor.karsay@gmx.at])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.3])
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

GTK_MIN_VERSION=3.12.0
PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= $GTK_MIN_VERSION, gmodule-2.0, gstreamer-1.0])
PACKAGE_CFLAGS="-std=c99 $GTK_CFLAGS"
PACKAGE_LIBS="$GTK_LIBS"
PACKAGE_LDFLAGS="$GTK_LDFLAGS"

AC_SUBST([PACKAGE_CFLAGS])
AC_SUBST([PACKAGE_LIBS])
AC_SUBST([PACKAGE_LDFLAGS])

AC_CONFIG_FILES([Makefile
                 data/Makefile
		 data/parlatype.desktop.in
		 data/icons/Makefile
		 docs/Makefile
		 po/Makefile.in
                 src/Makefile])
AC_OUTPUT

dnl ==========================================================================
echo "

        Parlatype $VERSION
        ==========================

        prefix:         ${prefix}

        Now type 'make' to build $PACKAGE
"
