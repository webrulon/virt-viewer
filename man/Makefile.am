NULL =

man_MANS =					\
	remote-viewer.1				\
	virt-viewer.1				\
	$(NULL)

EXTRA_DIST =					\
	remote-viewer.pod			\
	virt-viewer.pod				\
	$(NULL)

DISTCLEANFILES = $(man_MANS)

%.1: %.pod
	pod2man $< > $@

-include $(top_srcdir)/git.mk
