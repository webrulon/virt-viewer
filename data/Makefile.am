NULL =
CLEANFILES =

EXTRA_DIST =					\
	spice-xpi-client-remote-viewer		\
	virt-viewer.wxs.in			\
	virt-viewer.nsis.in			\
	virt-viewer-debug.nsis.in		\
	$(NULL)

if OS_WIN32

deps.txt:
	$(AM_V_GEN)rpm -qa | grep $(host_os) | sort | unix2dos > $@

virt-viewer-$(VERSION).exe: virt-viewer.nsis deps.txt
	$(AM_V_GEN)DESTDIR=`mktemp -d` && \
	make -C $(top_builddir) install DESTDIR=$$DESTDIR >/dev/null && \
	makensis -NOCD -DDESTDIR=$$DESTDIR $< >/dev/null && \
	rm -rf $$DESTDIR

virt-viewer-$(VERSION).msi: virt-viewer.wxs deps.txt
	$(AM_V_GEN)DESTDIR=`mktemp -d` && \
	make -C $(top_builddir) install DESTDIR=$$DESTDIR >/dev/null && \
	find $$DESTDIR | wixl-heat -p $$DESTDIR$(prefix)/ \
	    --component-group CG.virt-viewer --var var.DESTDIR \
	    --directory-ref=INSTALLDIR > virt-viewer-files.wxs && \
	wixl -D SourceDir=$(prefix) \
             -D DESTDIR=$$DESTDIR$(prefix) -o $@ \
	     $< virt-viewer-files.wxs && \
	rm -rf $$DESTDIR virt-viewer-files.wxs

CLEANFILES +=					\
	deps.txt				\
	virt-viewer-$(VERSION).exe		\
	virt-viewer-$(VERSION).msi		\
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

CLEANFILES += $(mime_DATA) $(desktop_DATA)
EXTRA_DIST += $(MIMEFILES) $(DESKTOPFILES)

endif

-include $(top_srcdir)/git.mk
