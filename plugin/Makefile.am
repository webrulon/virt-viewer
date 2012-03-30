if ENABLE_PLUGIN

plugindir       = $(libdir)/mozilla/plugins
plugin_LTLIBRARIES = virt-viewer-plugin.la

virt_viewer_plugin_la_SOURCES = \
	../src/virt-viewer-util.c ../src/virt-viewer-util.h \
	../src/virt-viewer-auth.c ../src/virt-viewer-auth.h \
	../src/virt-viewer.c ../src/virt-viewer.h \
	../src/virt-viewer-events.c ../src/virt-viewer-events.h \
	virt-viewer-plugin.c virt-viewer-plugin.h \
	npshell.c npunix.c
virt_viewer_plugin_la_LIBADD = \
	$(FIREFOX_PLUGIN_LIBS) \
	$(GTK_VNC_LIBS) \
	$(GTK_LIBS) \
	$(LIBXML2_LIBS) \
	$(LIBVIRT_LIBS)
virt_viewer_plugin_la_LDFLAGS = \
	-module -avoid-version
virt_viewer_plugin_la_CFLAGS = \
	-DPLUGIN=1 -DENABLE_DEBUG=1 \
	$(FIREFOX_PLUGIN_CFLAGS) \
	$(GTK_VNC_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	$(LIBVIRT_CFLAGS) \
	-DGLADE_DIR="\"$(gladedir)\"" \
	-I$(top_srcdir)/src

all-local: virt-viewer-plugin.so

virt-viewer-plugin.so: virt-viewer-plugin.la
	cp .libs/virt-viewer-plugin.so $@

# Only leave the .so file in the plugins directory.
install-data-hook:
	rm -f $(DESTDIR)$(plugindir)/virt-viewer-plugin.a $(DESTDIR)$(plugindir)/virt-viewer-plugin.la

CLEANFILES = virt-viewer-plugin.so

endif

-include $(top_srcdir)/git.mk
