
bin_PROGRAMS = virt-viewer

gladedir = $(pkgdatadir)/ui
glade_DATA = viewer.glade about.glade auth.glade

EXTRA_DIST = $(glade_DATA)

virt_viewer_SOURCES = \
	main.c \
	util.h util.c \
	auth.h auth.c \
	events.h events.c \
	viewer.h viewer.c \
	viewer-priv.h \
	display.h display.c \
	display-vnc.h display-vnc.c

if HAVE_SPICE_GTK
virt_viewer_SOURCES += \
	display-spice.h display-spice.c
endif

virt_viewer_LDADD = \
	@GTKVNC_LIBS@ \
	@SPICEGTK_LIBS@ \
	@GTK2_LIBS@ \
	@LIBXML2_LIBS@ \
	@LIBGLADE2_LIBS@ \
	@LIBVIRT_LIBS@

virt_viewer_CFLAGS = \
	@GTKVNC_CFLAGS@ \
	@SPICEGTK_CFLAGS@ \
	@GTK2_CFLAGS@ \
	@LIBXML2_CFLAGS@ \
	@LIBGLADE2_CFLAGS@ \
	@LIBVIRT_CFLAGS@ \
	@WARN_CFLAGS@ \
	-DGLADE_DIR="\"$(gladedir)\"" \
	-DLOCALE_DIR=\""$(datadir)/locale"\"
