
AC_INIT(virt-viewer, 0.4.2)
AC_CONFIG_SRCDIR(src/virt-viewer-main.c)
AM_CONFIG_HEADER(config.h)
dnl Make automake keep quiet about wildcards & other GNUmake-isms
AM_INIT_AUTOMAKE([-Wno-portability])
AC_CANONICAL_HOST

# Use the silent-rules feature when possible.
m4_ifndef([AM_SILENT_RULES], [m4_define([AM_SILENT_RULES],[])])
AM_SILENT_RULES([yes])

GMODULE2_REQUIRED=2.12.0
LIBXML2_REQUIRED="2.6.0"
LIBVIRT_REQUIRED="0.6.0"
GTK2_REQUIRED="2.12.0"
GTK3_REQUIRED="3.0"
GTK_VNC_REQUIRED="0.4.3"
SPICE_GTK_REQUIRED="0.6"

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


PKG_CHECK_MODULES(GMODULE2, gmodule-export-2.0 >= $GMODULE2_REQUIRED)
PKG_CHECK_MODULES(LIBXML2, libxml-2.0 >= $LIBXML2_REQUIRED)
PKG_CHECK_MODULES(LIBVIRT, libvirt >= $LIBVIRT_REQUIRED)

AC_MSG_CHECKING([which gtk+ version to compile against])
AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk=2.0|3.0],[which gtk+ version to compile against (default: 2.0)])],
  [case "$with_gtk" in
     2.0|3.0) ;;
     *) AC_MSG_ERROR([invalid gtk version specified]) ;;
   esac],
  [with_gtk=2.0])
AC_MSG_RESULT([$with_gtk])

case "$with_gtk" in
  2.0) GTK_API_VERSION=2.0
       GTK_REQUIRED=$GTK2_REQUIRED
       GTK_VNC_API_VERSION=1.0
       SPICE_GTK_API_VERSION=2.0
       ;;
  3.0) GTK_API_VERSION=3.0
       GTK_REQUIRED=$GTK3_REQUIRED
       GTK_VNC_API_VERSION=2.0
       SPICE_GTK_API_VERSION=3.0
       ;;
esac

AC_SUBST([GTK_API_VERSION])
AC_SUBST([GTK_REQUIRED])
AC_SUBST([GTK_VNC_API_VERSION])
AM_CONDITIONAL([HAVE_GTK_2],[test "$with_gtk" = "2.0"])
AM_CONDITIONAL([HAVE_GTK_3],[test "$with_gtk" = "3.0"])

PKG_CHECK_MODULES(GTK, gtk+-$GTK_API_VERSION >= $GTK_REQUIRED)

AC_ARG_WITH([gtk-vnc],
    AS_HELP_STRING([--without-gtk-vnc], [Ignore presence of gtk-vnc and disable it]))

AS_IF([test "x$with_gtk_vnc" != "xno"],
      [PKG_CHECK_MODULES(GTK_VNC,
                         gtk-vnc-$GTK_VNC_API_VERSION >= $GTK_VNC_REQUIRED,
                         [have_gtk_vnc=yes], [have_gtk_vnc=no])],
      [have_gtk_vnc=no])

AS_IF([test "x$have_gtk_vnc" = "xyes"],
      [AC_DEFINE([HAVE_GTK_VNC], 1, [Have gtk-vnc?])],
      [AS_IF([test "x$with_gtk_vnc" = "xyes"],
             [AC_MSG_ERROR([gtk-vnc requested but not found])
      ])
])
AM_CONDITIONAL([HAVE_GTK_VNC], [test "x$have_gtk_vnc" = "xyes"])

AC_ARG_WITH([spice-gtk],
    AS_HELP_STRING([--without-spice-gtk], [Ignore presence of spice-gtk and disable it]))

AS_IF([test "x$with_spice_gtk" != "xno"],
      [PKG_CHECK_MODULES(SPICE_GTK,
                         spice-client-gtk-$SPICE_GTK_API_VERSION >= $SPICE_GTK_REQUIRED,
                         [have_spice_gtk=yes], [have_spice_gtk=no])],
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

AC_MSG_NOTICE([])
AC_MSG_NOTICE([Configuration summary])
AC_MSG_NOTICE([=====================])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([ Features:])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([   Plugin: $enable_plugin])
AC_MSG_NOTICE([      Gtk: $with_gtk])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([ Libraries:])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([    GMODULE2: $GMODULE2_CFLAGS $GMODULE2_LIBS])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([         GTK: $GTK_CFLAGS $GTK_LIBS])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([     GTK_VNC: $GTK_VNC_CFLAGS $GTK_VNC_LIBS])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([   SPICE_GTK: $SPICE_GTK_CFLAGS $SPICE_GTK_LIBS])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([     LIBXML2: $LIBXML2_CFLAGS $LIBXML2_LIBS])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([     LIBVIRT: $LIBVIRT_CFLAGS $LIBVIRT_LIBS])
AC_MSG_NOTICE([])
