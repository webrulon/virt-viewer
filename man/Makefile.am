
man_MANS = virt-viewer.1

EXTRA_DIST = virt-viewer.pod
DISTCLEANFILES = $(man_MANS)

%.1: %.pod
	pod2man $< > $@
