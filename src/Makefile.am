NULL =
bin_PROGRAMS =

builderxmldir = $(pkgdatadir)/ui
builderxml_DATA =				\
	virt-viewer.xml				\
	virt-viewer-about.xml			\
	virt-viewer-auth.xml			\
	$(NULL)

EXTRA_DIST = $(builderxml_DATA) \
	gbinding.c \
	gbinding.h

COMMON_SOURCES =					\
	virt-gtk-compat.h				\
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

if HAVE_GTK_VNC
COMMON_SOURCES +=						\
	virt-viewer-session-vnc.h virt-viewer-session-vnc.c	\
	virt-viewer-display-vnc.h virt-viewer-display-vnc.c	\
	$(NULL)
endif

if HAVE_SPICE_GTK
COMMON_SOURCES +=						\
	virt-viewer-session-spice.h virt-viewer-session-spice.c	\
	virt-viewer-display-spice.h virt-viewer-display-spice.c	\
	$(NULL)
endif


if HAVE_LIBVIRT
bin_PROGRAMS += virt-viewer
virt_viewer_SOURCES =					\
	$(COMMON_SOURCES)				\
	virt-viewer-events.h virt-viewer-events.c	\
	virt-viewer.h virt-viewer.c			\
	virt-viewer-main.c				\
	$(NULL)
virt_viewer_LDFLAGS = 				\
	$(GMODULE2_LIBS)			\
	$(GTK_LIBS)				\
	$(LIBXML2_LIBS)				\
	$(LIBVIRT_LIBS)				\
	$(NULL)
virt_viewer_CFLAGS = 				\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(GMODULE2_CFLAGS) 			\
	$(GTK_CFLAGS)				\
	$(LIBXML2_CFLAGS)			\
	$(LIBVIRT_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(NULL)

if HAVE_GTK_VNC
virt_viewer_LDFLAGS += $(GTK_VNC_LIBS)
virt_viewer_CFLAGS += $(GTK_VNC_CFLAGS)
endif
if HAVE_SPICE_GTK
virt_viewer_LDFLAGS += $(SPICE_GTK_LIBS)
virt_viewer_CFLAGS += $(SPICE_GTK_CFLAGS)
endif
endif


bin_PROGRAMS += remote-viewer
remote_viewer_SOURCES =				\
	$(COMMON_SOURCES)			\
	remote-viewer.h remote-viewer.c		\
	remote-viewer-main.c			\
	$(NULL)
remote_viewer_LDFLAGS = 			\
	$(GMODULE2_LIBS)			\
	$(GTK_LIBS)				\
	$(LIBXML2_LIBS)				\
	$(NULL)
remote_viewer_CFLAGS =				\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(GMODULE2_CFLAGS) 			\
	$(GTK_CFLAGS)				\
	$(LIBXML2_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(NULL)

if HAVE_GTK_VNC
remote_viewer_LDFLAGS += $(GTK_VNC_LIBS)
remote_viewer_CFLAGS += $(GTK_VNC_CFLAGS)
endif
if HAVE_SPICE_GTK
remote_viewer_LDFLAGS += $(SPICE_GTK_LIBS) $(SPICE_CONTROLLER_LIBS)
remote_viewer_CFLAGS += $(SPICE_GTK_CFLAGS) $(SPICE_CONTROLLER_CFLAGS)
endif
