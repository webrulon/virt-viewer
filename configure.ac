AC_INIT(virt-viewer.spec.in)
AM_INIT_AUTOMAKE(virt-viewer, 0.0.2)

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

VIRT_VIEWER_COMPILE_WARNINGS(maximum)

PKG_CHECK_MODULES(LIBXML2, libxml-2.0 >= 2.6.0)
PKG_CHECK_MODULES(LIBVIRT, libvirt >= 0.2.0)
PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.2.0)
PKG_CHECK_MODULES(GTKVNC, gtk-vnc-1.0 >= 0.0.1)

dnl --enable-plugin to enable the browser plugin.
NSPR_REQUIRED=4.0.0
FIREFOX_PLUGIN_REQUIRED=2.0.0
AC_ARG_ENABLE(plugin,
        [  --enable-plugin=[no/yes] enable browser plugin [default=no]],,
        enable_plugin=no)
if test "x$enable_plugin" = "xyes"; then
    dnl Check for Netscape Portable Runtime development package.
    PKG_CHECK_MODULES(NSPR, nspr >= $NSPR_REQUIRED)
    dnl Check for Firefox plugin package (also includes NSPR cflags/libs).
    PKG_CHECK_MODULES(FIREFOX_PLUGIN,
                      firefox-plugin >= $FIREFOX_PLUGIN_REQUIRED)
    AC_SUBST(FIREFOX_PLUGIN_CFLAGS)
    AC_SUBST(FIREFOX_PLUGIN_LIBS)
fi
AM_CONDITIONAL(ENABLE_PLUGIN, [test "x$enable_plugin" = "xyes"])

AC_OUTPUT(Makefile
          src/Makefile
          man/Makefile
	  plugin/Makefile
          virt-viewer.spec)
