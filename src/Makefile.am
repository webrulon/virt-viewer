NULL =
# we are forced to use a shared library,
# because libtool reordred .a after .la libs, and it fails to link
noinst_LTLIBRARIES = libvirt-viewer.la
bin_PROGRAMS =

builderxmldir = $(pkgdatadir)/ui
builderxml_DATA =				\
	virt-viewer.xml				\
	virt-viewer-about.xml			\
	virt-viewer-auth.xml			\
	$(NULL)

EXTRA_DIST = $(builderxml_DATA)

if HAVE_LIBVIRT
bin_PROGRAMS += virt-viewer
virt_viewer_SOURCES =					\
	virt-viewer-events.h virt-viewer-events.c	\
	virt-viewer.h virt-viewer.c			\
	virt-viewer-main.c				\
	$(NULL)
virt_viewer_LDADD = libvirt-viewer.la
virt_viewer_LDFLAGS = 				\
	$(LIBVIRT_LIBS)				\
	$(GMODULE2_LIBS)			\
	$(NULL)
virt_viewer_CFLAGS = $(AM_CFLAGS) $(LIBVIRT_CFLAGS)
endif

bin_PROGRAMS += remote-viewer
remote_viewer_SOURCES =				\
	remote-viewer.h remote-viewer.c		\
	remote-viewer-main.c			\
	$(NULL)
remote_viewer_LDADD = libvirt-viewer.la
remote_viewer_LDFLAGS = 			\
	$(GMODULE2_LIBS)			\
	$(NULL)

libvirt_viewer_la_SOURCES =				\
	virt-viewer-util.h virt-viewer-util.c		\
	virt-viewer-auth.h virt-viewer-auth.c		\
	virt-viewer-app.h virt-viewer-app.c		\
	virt-viewer-session.h virt-viewer-session.c	\
	virt-viewer-display.h virt-viewer-display.c	\
	virt-viewer-notebook.h virt-viewer-notebook.c	\
	virt-viewer-window.h virt-viewer-window.c	\
	view/autoDrawer.c				\
	view/autoDrawer.h				\
	view/drawer.c					\
	view/drawer.h					\
	view/ovBox.c					\
	view/ovBox.h					\
	$(NULL)
libvirt_viewer_la_LIBADD =			\
	$(GMODULE2_LIBS)			\
	$(GTK_LIBS)				\
	$(LIBXML2_LIBS)				\
	$(NULL)

if HAVE_GTK_VNC
libvirt_viewer_la_SOURCES +=					\
	virt-viewer-session-vnc.h virt-viewer-session-vnc.c	\
	virt-viewer-display-vnc.h virt-viewer-display-vnc.c	\
	$(NULL)
libvirt_viewer_la_LIBADD += $(GTK_VNC_LIBS)
endif

if HAVE_SPICE_GTK
libvirt_viewer_la_SOURCES +=					\
	virt-viewer-session-spice.h virt-viewer-session-spice.c	\
	virt-viewer-display-spice.h virt-viewer-display-spice.c	\
	$(NULL)
libvirt_viewer_la_LIBADD += $(SPICE_GTK_LIBS)
endif

AM_CFLAGS =						\
	$(GMODULE2_CFLAGS) 				\
	$(GTK_CFLAGS)					\
	$(GTK_VNC_CFLAGS)				\
	$(LIBXML2_CFLAGS)				\
	$(SPICE_GTK_CFLAGS)				\
	$(WARN_CFLAGS)					\
	-DLOCALE_DIR=\""$(datadir)/locale"\"		\
	$(NULL)
