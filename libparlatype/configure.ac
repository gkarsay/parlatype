#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libparlatype], [1.5.6], [https://github.com/gkarsay/parlatype/issues])
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_CONFIG_SRCDIR([src/pt-player.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])
AC_SUBST([GETTEXT_PACKAGE], AC_PACKAGE_NAME)

AM_SILENT_RULES([yes])

LT_INIT([disable-static])
# Define these substitions here to keep all version information in one place.
# For information on how to properly maintain the library version information,
# refer to the libtool manual, section "Updating library version information":
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST([PARLATYPE_SO_VERSION], [1:6:0])

# Parlatype is unstable, we don't use API version yet
AC_SUBST([PARLATYPE_API_VERSION], [1.0])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([atexit])
AC_SEARCH_LIBS([ceil], [m])

GLIB_GSETTINGS                 
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])

GTK_MIN_VERSION=3.16.0
GST_MIN_VERSION=1.6.3
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= $GTK_MIN_VERSION, gmodule-2.0])
PKG_CHECK_MODULES([GSTREAMER], [gstreamer-1.0 >= $GST_MIN_VERSION, gstreamer-audio-1.0])

WARNINGCFLAGS="-Wall"
AC_SUBST([WARNINGCFLAGS])

GTK_DOC_CHECK([1.14],[--flavour no-tmpl])
GOBJECT_INTROSPECTION_CHECK([1.30.0])

# Gladeui catalog file, lib can be installed in system libdir since 3.12.2
AC_MSG_CHECKING([whether to install glade catalog])
AC_ARG_ENABLE([glade-catalog],
	[AS_HELP_STRING([--enable-glade-catalog],
		[install a glade catalog file [default=no]])],
	[glade_catalog=$enableval],
	[glade_catalog=no])
AC_MSG_RESULT([$glade_catalog])

AM_CONDITIONAL([GLADE_CATALOG], [ test "$glade_catalog" = "yes" ])

AS_IF([ test "$glade_catalog" = "yes" ],
	[PKG_CHECK_MODULES(GLADE, [gladeui-2.0 >= 3.12.2])
	 AC_MSG_CHECKING([where to install glade catalog])
	 GLADE_CATALOG_DIR=`$PKG_CONFIG --variable=catalogdir gladeui-2.0`
	 AC_MSG_RESULT([$GLADE_CATALOG_DIR])
	 AC_MSG_CHECKING([where to install glade icons])
	 GLADE_PIXMAP_DIR=`$PKG_CONFIG --variable=pixmapdir gladeui-2.0`
	 AC_MSG_RESULT([$GLADE_PIXMAP_DIR])
	 AC_SUBST(GLADE_PIXMAP_DIR)
	 AC_SUBST(GLADE_CATALOG_DIR)])

AC_ARG_ENABLE([tests],
	[AS_HELP_STRING([--enable-tests], [build tests [default=yes]])],
	[tests=$enableval],
	[tests=yes])

AM_CONDITIONAL(WITH_TESTS, [test "$tests" = "yes"])

# Check if rpath is disabled
AC_MSG_CHECKING(whether to use rpath)
AC_ARG_ENABLE(rpath,
	[AS_HELP_STRING([--enable-rpath],
			[Whether to set rpath in the libraries produced [default=yes]])],
	[libparlatype_use_rpath=$enableval],
	[libparlatype_use_rpath=yes])
AC_MSG_RESULT($libparlatype_use_rpath)

# Patch libtool to not use rpath if requested.
#
AC_CONFIG_COMMANDS([libtool-rpath-patch],
[if test "$libtool_patch_use_rpath" = "no"; then
   echo config.status: patching libtool to not use rpath
   sed < libtool > libtool-2 's/^hardcode_libdir_flag_spec.*$'/'hardcode_libdir_flag_spec=" -D__LIBTOOL_NO_RPATH__ "/'
   mv libtool-2 libtool
   chmod 755 libtool
fi],
[libtool_patch_use_rpath=$libparlatype_use_rpath])

GLIB_TESTS
AX_VALGRIND_DFLT([memcheck], [on])
AX_VALGRIND_CHECK
AX_CODE_COVERAGE

AC_CONFIG_FILES([Makefile
                 data/Makefile
		 data/parlatype.pc
		 data/icons/Makefile
		 docs/Makefile
		 docs/reference/Makefile
		 docs/reference/version.xml
		 po/Makefile.in
                 src/Makefile
                 tests/Makefile])

AC_OUTPUT

dnl ==========================================================================
echo "

        Common configuration
        ==========================

        prefix .............: ${prefix}
        compiler ...........: ${CC} `${CC} -dumpversion`


        libparlatype $VERSION
        ==========================

        enable-introspection: $enable_introspection
        enable-glade-catalog: ${glade_catalog}
        enable-gtk-doc .....: $enable_gtk_doc
        enable-tests .......: ${tests}
        enable-code-coverage: $enable_code_coverage
"
