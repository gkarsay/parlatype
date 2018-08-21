#!/bin/sh
# Run this to generate all the initial makefiles, etc.

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

OLDDIR=`pwd`
cd $srcdir

AUTORECONF=`which autoreconf`
if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please install it ***"
        exit 1
fi

GNOMEDOC=`which yelp-build`
if test -z $GNOMEDOC; then
        echo "*** The tools to build the documentation are not found,"
        echo "    please install the yelp-tools package ***"
        exit 1
fi

mkdir -p m4

gtkdocize --copy || exit 1

# libparlatype is a nested package
cd libparlatype
mkdir -p m4
gtkdocize --copy || exit 1
cd ..

autoreconf --force --install --verbose

cd $OLDDIR
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
