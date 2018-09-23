#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AX_REQUIRE_DEFINED([APPSTREAM_XML])
AX_REQUIRE_DEFINED([GLIB_GSETTINGS])
AX_REQUIRE_DEFINED([YELP_HELP_INIT])

AC_PREREQ([2.69])
AC_INIT([parlatype], [1.5.6], [https://github.com/gkarsay/parlatype/issues], [parlatype], [https://gkarsay.github.io/parlatype/])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])
AC_SUBST([GETTEXT_PACKAGE], AC_PACKAGE_NAME)

AM_SILENT_RULES([yes])

LT_INIT([disable-static])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([atexit setlocale])

GLIB_GSETTINGS                 
APPSTREAM_XML
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])
AC_PATH_PROG([DESKTOP_FILE_VALIDATE], [desktop-file-validate], [/bin/true])
AC_PATH_PROG([APPDATA_VALIDATE], [appstream-util], [/bin/true])

YELP_HELP_INIT

GTK_MIN_VERSION=3.16.0
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= $GTK_MIN_VERSION, gmodule-2.0])

WARNINGCFLAGS="-Wall"
AC_SUBST([WARNINGCFLAGS])

# Check if rpath is disabled
AC_MSG_CHECKING(whether to use rpath)
AC_ARG_ENABLE(rpath,
	[AS_HELP_STRING([--enable-rpath],
			[Whether to set rpath in the libraries produced [default=yes]])],
	[parlatype_use_rpath=$enableval],
	[parlatype_use_rpath=yes])
AC_MSG_RESULT($parlatype_use_rpath)

# Patch libtool to not use rpath if requested.
#
AC_CONFIG_COMMANDS([libtool-rpath-patch],
[if test "$libtool_patch_use_rpath" = "no"; then
   echo config.status: patching libtool to not use rpath
   sed < libtool > libtool-2 's/^hardcode_libdir_flag_spec.*$'/'hardcode_libdir_flag_spec=" -D__LIBTOOL_NO_RPATH__ "/'
   mv libtool-2 libtool
   chmod 755 libtool
fi],
[libtool_patch_use_rpath=$parlatype_use_rpath])

AC_ARG_WITH([libreoffice],
            [AS_HELP_STRING([--without-libreoffice],
              [don't install LibreOffice macros])],
            [],
            [with_libreoffice=yes])

AM_CONDITIONAL([LIBREOFFICE], [test "$with_libreoffice" = "yes"])

# gtk-doc only needed with --enable-gtk-doc
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])
],[
AM_CONDITIONAL([ENABLE_GTK_DOC], false)
])

AC_CONFIG_FILES([Makefile
                 data/Makefile
		 data/icons/Makefile
		 docs/Makefile
		 help/Makefile
		 libreoffice/Makefile
		 po/Makefile.in
                 src/Makefile])

AC_CONFIG_SUBDIRS([libparlatype])
AC_OUTPUT

dnl ==========================================================================
echo "
        Parlatype $VERSION
        ==========================

	with-libreoffice ...: $with_libreoffice
"

if test "${prefix}" != "/usr" -a "$with_libreoffice" == "yes"; then
echo "
        Note: Consider using --without-libreoffice for prefixes
        other than /usr and copying the script manually to:
        /usr/lib/libreoffice/share/Scripts/python/Parlatype.py
        On some systems it's lib64 or lib32 instead of lib.
"
fi
