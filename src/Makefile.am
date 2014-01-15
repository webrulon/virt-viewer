NULL =
LDADD =
MAINTAINERCLEANFILES =
bin_PROGRAMS =

builderxmldir = $(pkgdatadir)/ui
builderxml_DATA =				\
	virt-viewer.xml				\
	virt-viewer-about.xml			\
	virt-viewer-auth.xml			\
	$(NULL)

EXTRA_DIST =					\
	$(builderxml_DATA)			\
	virt-viewer-enums.c.etemplate		\
	virt-viewer-enums.h.etemplate		\
	gbinding.c				\
	gbinding.h				\
	$(NULL)

ENUMS_FILES =					\
	virt-viewer-display.h			\
	$(NULL)

BUILT_SOURCES =					\
	virt-viewer-enums.h			\
	virt-viewer-enums.c			\
	$(NULL)

$(BUILT_SOURCES): %: %.etemplate $(ENUMS_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $^ | \
            sed -e 's/VIRT_TYPE_VIEWER/VIRT_VIEWER_TYPE/' \
                -e 's,#include "$(srcdir)/,#include ",' > $@

COMMON_SOURCES =					\
	$(BUILT_SOURCES)				\
	virt-glib-compat.h				\
	virt-glib-compat.c				\
	virt-gtk-compat.h				\
	virt-viewer-util.h virt-viewer-util.c		\
	virt-viewer-auth.h virt-viewer-auth.c		\
	virt-viewer-app.h virt-viewer-app.c		\
	virt-viewer-file.h virt-viewer-file.c		\
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
virt_viewer_LDFLAGS =				\
	-lm					\
	$(GLIB2_LIBS)				\
	$(GTK_LIBS)				\
	$(GTK_VNC_LIBS)				\
	$(LIBXML2_LIBS)				\
	$(LIBVIRT_LIBS)				\
	$(OVIRT_LIBS)				\
	$(SPICE_GTK_LIBS)			\
	$(NULL)
virt_viewer_CFLAGS = 				\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"virt-viewer\"          \
	$(GLIB2_CFLAGS) 			\
	$(GTK_CFLAGS)				\
	$(GTK_VNC_CFLAGS)			\
	$(LIBXML2_CFLAGS)			\
	$(LIBVIRT_CFLAGS)			\
	$(OVIRT_CFLAGS)				\
	$(SPICE_GTK_CFLAGS)			\
	$(SPICE_CONTROLLER_CFLAGS)		\
	$(WARN_CFLAGS)				\
	$(NULL)
endif


bin_PROGRAMS += remote-viewer
remote_viewer_SOURCES =				\
	$(COMMON_SOURCES)			\
	remote-viewer.h remote-viewer.c		\
	remote-viewer-main.c			\
	$(NULL)
remote_viewer_LDFLAGS =				\
	-lm					\
	$(GLIB2_LIBS)				\
	$(GTK_LIBS)				\
	$(GTK_VNC_LIBS)				\
	$(LIBXML2_LIBS)				\
	$(OVIRT_LIBS)				\
	$(SPICE_GTK_LIBS)			\
	$(SPICE_CONTROLLER_LIBS)		\
	$(NULL)
remote_viewer_CFLAGS =				\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	-DG_LOG_DOMAIN=\"remote-viewer\"        \
	$(GLIB2_CFLAGS) 			\
	$(GTK_CFLAGS)				\
	$(GTK_VNC_CFLAGS)			\
	$(LIBXML2_CFLAGS)			\
	$(OVIRT_CFLAGS)				\
	$(SPICE_GTK_CFLAGS)			\
	$(SPICE_CONTROLLER_CFLAGS)		\
	$(WARN_CFLAGS)				\
	$(NULL)

if OS_WIN32
remote_viewer_LDFLAGS += -Wl,--subsystem,windows
endif

AM_CPPFLAGS = -DPACKAGE_DATADIR=\""$(pkgdatadir)"\"

VIRT_VIEWER_RES = virt-viewer.rc virt-viewer.manifest
ICONDIR = $(top_builddir)/icons
MANIFESTDIR = $(srcdir)
EXTRA_DIST += $(VIRT_VIEWER_RES)

if OS_WIN32
bin_PROGRAMS += windows-cmdline-wrapper
windows_cmdline_wrapper_SOURCES = windows-cmdline-wrapper.c
windows_cmdline_wrapper_LDFLAGS = -lpsapi

virt-viewer_rc.$(OBJEXT): $(VIRT_VIEWER_RES) $(ICONDIR)/virt-viewer.ico
	$(AM_V_GEN)$(WINDRES)				\
		-DICONDIR='\"$(ICONDIR)\"'		\
		-DMANIFESTDIR='\"$(MANIFESTDIR)\"'	\
		-i $< -o $@
LDADD += virt-viewer_rc.$(OBJEXT)
MAINTAINERCLEANFILES += virt-viewer_rc.$(OBJEXT)

bin_PROGRAMS += debug-helper
debug_helper_SOURCES = debug-helper.c
debug_helper_LDFLAGS = $(GLIB2_LIBS) -Wl,--subsystem,windows
debug_helper_CFLAGS = $(GLIB2_CFLAGS)
endif

-include $(top_srcdir)/git.mk
