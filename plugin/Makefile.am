if ENABLE_PLUGIN

plugindir       = $(libdir)/mozilla/plugins
plugin_LTLIBRARIES = virt-viewer-plugin.la

virt_viewer_plugin_la_SOURCES = \
	../src/main.c ../src/viewer.h \
	virt-viewer-plugin.c virt-viewer-plugin.h \
	npshell.c npunix.c
virt_viewer_plugin_la_LIBADD = \
	@FIREFOX_PLUGIN_LIBS@ \
	@GTKVNC_LIBS@ @GTK2_LIBS@ @LIBXML2_LIBS@ @LIBVIRT_LIBS@
virt_viewer_plugin_la_LDFLAGS = \
	-module -avoid-version
virt_viewer_plugin_la_CFLAGS = \
	-DPLUGIN=1 -DENABLE_DEBUG=1 -DDEBUG=1 \
	@FIREFOX_PLUGIN_CFLAGS@ \
	@GTKVNC_CFLAGS@ @GTK2_CFLAGS@ @LIBXML2_CFLAGS@ @LIBVIRT_CFLAGS@ \
	@WARN_CFLAGS@ \
	-I$(top_srcdir)/src

all-local: virt-viewer-plugin.so

virt-viewer-plugin.so: virt-viewer-plugin.la
	cp .libs/virt-viewer-plugin.so $@

# Only leave the .so file in the plugins directory.
install-data-hook:
	rm -f $(plugindir)/virt-viewer-plugin.a $(plugindir)/virt-viewer-plugin.la

CLEANFILES = virt-viewer-plugin.so

endif
