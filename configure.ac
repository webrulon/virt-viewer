AC_INIT(virt-viewer.spec.in)
AM_INIT_AUTOMAKE(virt-viewer, 0.0.2)

AC_PROG_CC

VIRT_VIEWER_COMPILE_WARNINGS(maximum)

PKG_CHECK_MODULES(LIBXML2, libxml-2.0 >= 2.6.0)
PKG_CHECK_MODULES(LIBVIRT, libvirt >= 0.2.0)
PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.2.0)
PKG_CHECK_MODULES(GTKVNC, gtk-vnc-1.0 >= 0.0.1)

AC_OUTPUT(Makefile
          src/Makefile
          man/Makefile
          virt-viewer.spec)
