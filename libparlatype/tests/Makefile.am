include $(top_srcdir)/glib-tap.mk

@VALGRIND_CHECK_RULES@
VALGRIND_SUPPRESSIONS_FILES = glib.supp gst.supp

@CODE_COVERAGE_RULES@
CODE_COVERAGE_IGNORE_PATTERN = pt-resources.c *include*

EXTRA_DIST += \
	glib.supp	\
	gst.supp	\
	data/README

AM_CFLAGS = -g $(GTK_CFLAGS) $(GSTREAMER_CFLAGS) $(MAINTAINER_CFLAGS)

LDADD = $(top_builddir)/src/libparlatype.la $(GTK_LIBS) $(GSTREAMER_LIBS)
AM_LDFLAGS = -export-dynamic
AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"libparlatype-unittest\"	\
	-I$(top_srcdir) 			\
	-I$(top_builddir)

test_programs = \
	player		\
	progressdialog	\
	wavedata	\
	waveloader	\
	waveviewer

dist_test_data = \
	data/test1.ogg
