NULL=

EXTRA_DIST =					\
	spice-xpi-client-remote-viewer		\
	virt-viewer.nsis.in			\
	virt-viewer-debug.nsis.in			\
	$(NULL)

if OS_WIN32
deps.txt:
	$(AM_V_GEN)rpm -qa | grep $(host_os) | sort | unix2dos > $@

virt-viewer-$(VERSION).exe: virt-viewer.nsis deps.txt
	$(AM_V_GEN)DESTDIR=`mktemp -d` && \
	make -C $(top_builddir) install DESTDIR=$$DESTDIR >/dev/null && \
	makensis -NOCD -DDESTDIR=$$DESTDIR $< >/dev/null && \
	rm -rf $$DESTDIR

CLEANFILES = deps.txt virt-viewer-$(VERSION).exe
endif

-include $(top_srcdir)/git.mk
