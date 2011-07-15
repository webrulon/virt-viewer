
bin_PROGRAMS = virt-viewer

builderxmldir = $(pkgdatadir)/ui
builderxml_DATA = \
	virt-viewer.xml \
	virt-viewer-about.xml \
	virt-viewer-auth.xml

EXTRA_DIST = $(builderxml_DATA)

virt_viewer_SOURCES = \
	virt-viewer-main.c \
	virt-viewer.h virt-viewer.c \
	virt-viewer-util.h virt-viewer-util.c \
	virt-viewer-auth.h virt-viewer-auth.c \
	virt-viewer-events.h virt-viewer-events.c \
	virt-viewer-app.h virt-viewer-app.c \
	virt-viewer-session.h virt-viewer-session.c \
	virt-viewer-session-vnc.h virt-viewer-session-vnc.c \
	virt-viewer-display.h virt-viewer-display.c \
	virt-viewer-display-vnc.h virt-viewer-display-vnc.c \
	virt-viewer-notebook.h virt-viewer-notebook.c \
	virt-viewer-window.h virt-viewer-window.c \
	view/autoDrawer.c \
	view/autoDrawer.h \
	view/drawer.c \
	view/drawer.h \
	view/ovBox.c \
	view/ovBox.h


if HAVE_SPICE_GTK
virt_viewer_SOURCES += \
	virt-viewer-session-spice.h virt-viewer-session-spice.c \
	virt-viewer-display-spice.h virt-viewer-display-spice.c
endif

virt_viewer_LDADD = \
	@GTK_VNC_LIBS@ \
	@SPICE_GTK_LIBS@ \
	@GLIB2_LIBS@ \
	@GTK_LIBS@ \
	@LIBXML2_LIBS@ \
	@LIBVIRT_LIBS@

virt_viewer_CFLAGS = \
	@GTK_VNC_CFLAGS@ \
	@SPICE_GTK_CFLAGS@ \
	@GLIB2_CFLAGS@ \
	@GTK_CFLAGS@ \
	@LIBXML2_CFLAGS@ \
	@LIBVIRT_CFLAGS@ \
	@WARN_CFLAGS@ \
	-DBUILDER_XML_DIR="\"$(builderxmldir)\"" \
	-DLOCALE_DIR=\""$(datadir)/locale"\"
