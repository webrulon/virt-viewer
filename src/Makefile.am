
bin_PROGRAMS = virt-viewer

builderxmldir = $(pkgdatadir)/ui
builderxml_DATA = viewer.xml about.xml auth.xml

EXTRA_DIST = $(builderxml_DATA)

virt_viewer_SOURCES = \
	main.c \
	util.h util.c \
	auth.h auth.c \
	events.h events.c \
	viewer.h viewer.c \
	viewer-priv.h \
	display.h display.c \
	display-vnc.h display-vnc.c \
	view/autoDrawer.c \
	view/autoDrawer.h \
	view/drawer.c \
	view/drawer.h \
	view/ovBox.c \
	view/ovBox.h


if HAVE_SPICE_GTK
virt_viewer_SOURCES += \
	display-spice.h display-spice.c
endif

virt_viewer_LDADD = \
	@GTKVNC_LIBS@ \
	@SPICEGTK_LIBS@ \
	@GLIB2_LIBS@ \
	@GTK2_LIBS@ \
	@LIBXML2_LIBS@ \
	@LIBVIRT_LIBS@

virt_viewer_CFLAGS = \
	@GTKVNC_CFLAGS@ \
	@SPICEGTK_CFLAGS@ \
	@GLIB2_CFLAGS@ \
	@GTK2_CFLAGS@ \
	@LIBXML2_CFLAGS@ \
	@LIBVIRT_CFLAGS@ \
	@WARN_CFLAGS@ \
	-DBUILDER_XML_DIR="\"$(builderxmldir)\"" \
	-DLOCALE_DIR=\""$(datadir)/locale"\"
