
AC_INIT(virt-viewer, 0.3.1)
AC_CONFIG_SRCDIR(src/main.c)
AM_CONFIG_HEADER(config.h)
dnl Make automake keep quiet about wildcards & other GNUmake-isms
AM_INIT_AUTOMAKE([-Wno-portability])
AC_CANONICAL_HOST

# Use the silent-rules feature when possible.
m4_ifndef([AM_SILENT_RULES], [m4_define([AM_SILENT_RULES],[])])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_CONFIG_LIBOBJ_DIR([src])

AC_DEFINE([_GNU_SOURCE], [], [Enable GNU extensions])

VIRT_VIEWER_COMPILE_WARNINGS(maximum)

GETTEXT_PACKAGE=virt-viewer
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

IT_PROG_INTLTOOL([0.35.0])
AM_GLIB_GNU_GETTEXT


PKG_CHECK_MODULES(LIBXML2, libxml-2.0 >= 2.6.0)
PKG_CHECK_MODULES(LIBVIRT, libvirt >= 0.6.0)
PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.10.0)
PKG_CHECK_MODULES(LIBGLADE2, libglade-2.0 >= 2.6.0)
PKG_CHECK_MODULES(GTKVNC, gtk-vnc-1.0 >= 0.3.8)

AC_ARG_WITH([spice-gtk],
    AS_HELP_STRING([--without-spice-gtk], [Ignore presence of spice-gtk and disable it]))

AS_IF([test "x$with_spice_gtk" != "xno"],
      [PKG_CHECK_MODULES(SPICEGTK, spice-client-gtk-2.0 >= 0.5, [have_spice_gtk=yes], [have_spice_gtk=no])],
      [have_spice_gtk=no])

AS_IF([test "x$have_spice_gtk" = "xyes"],
      [AC_DEFINE([HAVE_SPICE_GTK], 1, [Have spice-gtk?])],
      [AS_IF([test "x$with_spice_gtk" = "xyes"],
             [AC_MSG_ERROR([spice-gtk requested but not found])
      ])
])
AM_CONDITIONAL([HAVE_SPICE_GTK], [test "x$have_spice_gtk" = "xyes"])

dnl Decide if this platform can support the SSH tunnel feature.
AC_CHECK_HEADERS([sys/socket.h sys/un.h windows.h])
AC_CHECK_FUNCS([fork socketpair])

dnl --enable-plugin to enable the browser plugin.
NSPR_REQUIRED=4.0.0
FIREFOX_PLUGIN_REQUIRED=1.5.0
MOZILLA_PLUGIN_REQUIRED=1.8
AC_ARG_ENABLE(plugin,
        [  --enable-plugin=[no/yes] enable browser plugin [default=no]],,
        enable_plugin=no)
if test "x$enable_plugin" = "xyes"; then
    dnl Check for Netscape Portable Runtime development package.
    PKG_CHECK_MODULES(NSPR, nspr >= $NSPR_REQUIRED)
    dnl Check for Firefox plugin package (also includes NSPR cflags/libs).
    PKG_CHECK_MODULES(FIREFOX_PLUGIN,
                     firefox-plugin >= $FIREFOX_PLUGIN_REQUIRED, ,[
        PKG_CHECK_MODULES(MOZILLA_PLUGIN,
                          mozilla-plugin >= $MOZILLA_PLUGIN_REQUIRED)
        FIREFOX_PLUGIN_CFLAGS="$MOZILLA_PLUGIN_CFLAGS"
        FIREFOX_PLUGIN_LIBS="$MOZILLA_PLUGIN_LIBS"
    ])
    AC_SUBST(FIREFOX_PLUGIN_CFLAGS)
    AC_SUBST(FIREFOX_PLUGIN_LIBS)
fi
AM_CONDITIONAL(ENABLE_PLUGIN, [test "x$enable_plugin" = "xyes"])

AC_OUTPUT(Makefile
          src/Makefile
          man/Makefile
	  po/Makefile.in
	  plugin/Makefile
          virt-viewer.spec
          mingw32-virt-viewer.spec)
