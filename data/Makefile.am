NULL =
CLEANFILES =
MANUFACTURER = Virt Manager Project

EXTRA_DIST =					\
	spice-xpi-client-remote-viewer		\
	virt-viewer.wxs.in			\
	virt-viewer.nsis.in			\
	virt-viewer-debug.nsis.in		\
	$(NULL)

# this make sure those files are regenerated when they change
# (in maintainer-mode)
all-local: virt-viewer.wxs virt-viewer.nsis virt-viewer-debug.nsis

if OS_WIN32

deps.txt:
	$(AM_V_GEN)rpm -qa | grep $(host_os) | sort | unix2dos > $@

virt-viewer-$(VERSION).exe: virt-viewer.nsis deps.txt
	$(AM_V_GEN)DESTDIR=`mktemp -d` && \
	make -C $(top_builddir) install DESTDIR=$$DESTDIR >/dev/null && \
	makensis -NOCD -DDESTDIR=$$DESTDIR $< >/dev/null && \
	rm -rf $$DESTDIR

virt-viewer-debug-$(VERSION).exe: virt-viewer-debug.nsis
	$(AM_V_GEN)DESTDIR=`mktemp -d` && \
	make -C $(top_builddir) install DESTDIR=$$DESTDIR >/dev/null && \
	makensis -NOCD -DDESTDIR=$$DESTDIR $< >/dev/null && \
	rm -rf $$DESTDIR

virt-viewer-$(WIXL_ARCH)-$(VERSION).msi: virt-viewer.wxs deps.txt
	$(AM_V_GEN)DESTDIR=`mktemp -d` &&				\
	make -C $(top_builddir) install DESTDIR=$$DESTDIR >/dev/null &&	\
	find $$DESTDIR | wixl-heat -p $$DESTDIR$(prefix)/		\
	    --component-group CG.virt-viewer --var var.DESTDIR		\
	    --directory-ref=INSTALLDIR > virt-viewer-files.wxs &&	\
	MANUFACTURER="$(MANUFACTURER)" wixl -D SourceDir=$(prefix)	\
             -D DESTDIR=$$DESTDIR$(prefix)				\
             --arch $(WIXL_ARCH)					\
             -o $@							\
	     $< virt-viewer-files.wxs &&				\
	rm -rf $$DESTDIR virt-viewer-files.wxs

msi: virt-viewer-$(WIXL_ARCH)-$(VERSION).msi

CLEANFILES +=					\
	deps.txt				\
	virt-viewer-$(VERSION).exe		\
	virt-viewer-$(WIXL_ARCH)-$(VERSION).msi	\
	$(NULL)

else #!WIN32

desktopdir       = $(datadir)/applications
DESKTOPFILES     = remote-viewer.desktop.in
desktop_DATA     = $(DESKTOPFILES:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

MIMEFILES = virt-viewer-mime.xml.in
mimedir = $(datadir)/mime/packages
mime_DATA = virt-viewer-mime.xml
@INTLTOOL_XML_RULE@

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	@if $(AM_V_P); then set -x; else echo "  INSTALL  update-mime-database"; fi; \
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime";
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	@if $(AM_V_P); then set -x; else echo "  UNINSTALL update-mime-database"; fi; \
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime";
endif

CLEANFILES += $(mime_DATA) $(desktop_DATA)
EXTRA_DIST += $(MIMEFILES) $(DESKTOPFILES)

endif

-include $(top_srcdir)/git.mk
