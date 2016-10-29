#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libparlatype], [1.4], [gabor.karsay@gmx.at])
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_CONFIG_SRCDIR([src/pt-player.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST([GETTEXT_PACKAGE], AC_PACKAGE_NAME)
IT_PROG_INTLTOOL(0.50.2)
AM_SILENT_RULES([yes])

LT_INIT([disable-static])
# Define these substitions here to keep all version information in one place.
# For information on how to properly maintain the library version information,
# refer to the libtool manual, section "Updating library version information":
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST([PARLATYPE_SO_VERSION], [0:0:0])

# Parlatype is unstable, we don't use API version yet
AC_SUBST([PARLATYPE_API_VERSION], [1.0])

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

GTK_MIN_VERSION=3.10.0
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= $GTK_MIN_VERSION, gmodule-2.0])
PKG_CHECK_MODULES([GSTREAMER], [gstreamer-1.0])

WARNINGCFLAGS="-Wall"
AC_SUBST([WARNINGCFLAGS])

GTK_DOC_CHECK([1.14],[--flavour no-tmpl])
GOBJECT_INTROSPECTION_CHECK([1.30.0])

# Gladeui catalog file, lib can be installed in system libdir since 3.12.2
PKG_CHECK_MODULES([GLADEUI],[gladeui-2.0 >= 3.12.2])
glade_catalog_dir=`$PKG_CONFIG --variable=catalogdir gladeui-2.0`
glade_pixmap_dir=`$PKG_CONFIG --variable=pixmapdir gladeui-2.0`
AC_SUBST(GLADE_CATALOG_DIR, $glade_catalog_dir)
AC_SUBST(GLADE_PIXMAP_DIR, $glade_pixmap_dir)

AC_CONFIG_FILES([Makefile
                 data/Makefile
		 data/parlatype.pc
		 data/parlatype.xml
		 data/icons/Makefile
		 docs/Makefile
		 docs/reference/Makefile
		 docs/reference/version.xml
		 po/Makefile.in
                 src/Makefile])

AC_OUTPUT

dnl ==========================================================================
echo "

        libparlatype $VERSION
        ==========================

        prefix .......: ${prefix}
        enable-gtk-doc: $enable_gtk_doc

        Now type 'make' to build $PACKAGE
"
