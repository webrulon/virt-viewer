#!/bin/sh
# Run this to generate all the initial makefiles, etc.

set -e
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

THEDIR=`pwd`
cd $srcdir

EXTRA_ARGS=""
if test "x$1" = "x--system"; then
    shift
    prefix=/usr
    libdir=$prefix/lib
    sysconfdir=/etc
    localstatedir=/var
    if [ -d /usr/lib64 ]; then
        libdir=$prefix/lib64
    fi
    EXTRA_ARGS="--prefix=$prefix --sysconfdir=$sysconfdir --localstatedir=$localstatedir --libdir=$libdir"
fi

intltoolize --force
autoreconf -vfi

cd $THEDIR

if [ -z "$NOCONFIGURE" ]; then
    if test -z "$*" ; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
    fi
    $srcdir/configure $EXTRA_ARGS "$@" && {
        echo
        echo "Now type 'make' to compile virt-viewer."
    }
fi
