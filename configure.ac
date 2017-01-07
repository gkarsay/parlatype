#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([parlatype], [1.4.1], [gabor.karsay@gmx.at])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST([GETTEXT_PACKAGE], AC_PACKAGE_NAME)
IT_PROG_INTLTOOL(0.50.2)
AM_SILENT_RULES([yes])

LT_INIT([disable-static])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([atexit setlocale])

GLIB_GSETTINGS                 
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)
AC_PATH_PROG([DESKTOP_FILE_VALIDATE], [desktop-file-validate], [/bin/true])
AC_PATH_PROG([APPDATA_VALIDATE], [appstream-util], [/bin/true])

YELP_HELP_INIT

GTK_MIN_VERSION=3.10.0
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= $GTK_MIN_VERSION, gmodule-2.0])

WARNINGCFLAGS="-Wall"
AC_SUBST([WARNINGCFLAGS])

AC_ARG_WITH([lo],
            [AS_HELP_STRING([--without-lo],
              [don't install LibreOffice macros])],
            [],
            [with_lo=yes])

AM_CONDITIONAL([LO], [test "$with_lo" = "yes"])

# needed?
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

AC_CONFIG_FILES([Makefile
                 data/Makefile
		 data/icons/Makefile
		 docs/Makefile
		 help/Makefile
		 libreoffice/Makefile
		 libreoffice/Parlatype.py
		 po/Makefile.in
                 src/Makefile])

AC_CONFIG_SUBDIRS([libparlatype])
AC_OUTPUT

dnl ==========================================================================
echo "

        Parlatype $VERSION
        ==========================

        prefix .......: ${prefix}
	with-lo ......: $with_lo
"

if test "${prefix}" != "/usr" -a "$with_lo" == "yes"; then
echo "
        Note: Consider using --without-lo for prefixes
        other than /usr and copying the script manually to:
        /usr/lib/libreoffice/share/Scripts/python/Parlatype.py"
fi
