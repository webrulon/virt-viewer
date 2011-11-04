#!/bin/sh

set -e
set -v

# Make things clean.
test -f Makefile && make -k distclean || :

rm -rf build
mkdir build
cd build

../autogen.sh --prefix=$AUTOBUILD_INSTALL_ROOT \
    --enable-compile-warnings=error \
    --disable-plugin \
    --with-gtk=2.0

make
make install

# Test GTK3 build too if available
pkg-config gtk+-3.0 1>/dev/null 2>&1
if test $? = 0 ; then
  make distclean
  ../configure --prefix=$AUTOBUILD_INSTALL_ROOT \
    --enable-compile-warnings=error \
    --disable-plugin \
    --with-gtk=3.0
  make
  make install
fi

rm -f *.tar.gz
make dist

if [ -n "$AUTOBUILD_COUNTER" ]; then
  EXTRA_RELEASE=".auto$AUTOBUILD_COUNTER"
else
  NOW=`date +"%s"`
  EXTRA_RELEASE=".$USER$NOW"
fi

if [ -f /usr/bin/rpmbuild ]; then
  rpmbuild --nodeps \
     --define "extra_release $EXTRA_RELEASE" \
     --define "_sourcedir `pwd`" \
     -ba --clean virt-viewer.spec
fi

if [ -x /usr/bin/i686-pc-mingw32-gcc ]; then
  make distclean

  PKG_CONFIG_PATH="$AUTOBUILD_INSTALL_ROOT/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig:/usr/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig" \
  CC="i686-pc-mingw32-gcc" \
  ../configure \
    --build=$(uname -m)-pc-linux \
    --host=i686-pc-mingw32 \
    --prefix="$AUTOBUILD_INSTALL_ROOT/i686-pc-mingw32/sys-root/mingw" \
    --disable-plugin \
    --with-gtk=2.0

  make
  make install

  # Test GTK3 build too if available
  PKG_CONFIG_LIBDIR=/usr/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig pkg-config gtk+-3.0 1>/dev/null 2>&1
  if test $? = 0 ; then
    make distclean
    PKG_CONFIG_PATH="$AUTOBUILD_INSTALL_ROOT/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig:/usr/i686-pc-mingw32/sys-root/mingw/lib/pkgconfig" \
    CC="i686-pc-mingw32-gcc" \
    ../configure --prefix=$AUTOBUILD_INSTALL_ROOT \
      --build=$(uname -m)-pc-linux \
      --host=i686-pc-mingw32 \
      --prefix="$AUTOBUILD_INSTALL_ROOT/i686-pc-mingw32/sys-root/mingw" \
      --disable-plugin \
      --with-gtk=3.0
    make
    make install
  fi

  #set -o pipefail
  #make check 2>&1 | tee "$RESULTS"

  if [ -f /usr/bin/rpmbuild ]; then
    rpmbuild --nodeps \
       --define "extra_release $EXTRA_RELEASE" \
       --define "_sourcedir `pwd`" \
       -ba --clean mingw32-virt-viewer.spec
  fi
fi
