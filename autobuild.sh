#!/bin/sh

set -e
set -v

# Make things clean.
test -f Makefile && make -k distclean || :

rm -rf build
mkdir build
cd build

../autogen.sh --prefix=$AUTOBUILD_INSTALL_ROOT --enable-fatal-warnings

make
make install

rm -f *.tar.gz
make dist

if [ -f /usr/bin/rpmbuild ]; then
  if [ -n "$AUTOBUILD_COUNTER" ]; then
    EXTRA_RELEASE=".auto$AUTOBUILD_COUNTER"
  else
    NOW=`date +"%s"`
    EXTRA_RELEASE=".$USER$NOW"
  fi
  rpmbuild --nodeps --define "extra_release $EXTRA_RELEASE" -ta --clean *.tar.gz
fi
