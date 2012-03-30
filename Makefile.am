NULL =

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = icons src man plugin po data

EXTRA_DIST =					\
	$(PACKAGE).spec				\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	GNUmakefile				\
	maint.mk				\
	cfg.mk					\
	build-aux/gitlog-to-changelog		\
	build-aux/useless-if-before-free	\
	build-aux/vc-list-files			\
	$(NULL)

DISTCLEAN_FILES =				\
	$(PACKAGE).spec				\
	intltool-extract			\
	intltool-merge				\
	intltool-update				\
	$(NULL)

MAINTAINERCLEANFILES =				\
	$(srcdir)/INSTALL			\
	$(srcdir)/aclocal.m4			\
	$(srcdir)/autoscan.log			\
	$(srcdir)/config.h.in			\
	$(srcdir)/build-aux/compile		\
	$(srcdir)/build-aux/config.guess	\
	$(srcdir)/build-aux/config.sub		\
	$(srcdir)/build-aux/depcomp		\
	$(srcdir)/build-aux/install-sh		\
	$(srcdir)/build-aux/ltmain.sh		\
	$(srcdir)/build-aux/missing		\
	$(srcdir)/m4/intltool.m4		\
	$(srcdir)/m4/libtool.m4			\
	$(srcdir)/m4/ltoptions.m4		\
	$(srcdir)/m4/ltsugar.m4			\
	$(srcdir)/m4/ltversion.m4		\
	$(srcdir)/m4/lt~obsolete.m4		\
	$(NULL)

dist-hook: gen-ChangeLog

# Generate the ChangeLog file (with all entries since the switch to git)
# and insert it into the directory we're about to use to create a tarball.
.PHONY: gen-ChangeLog
gen-ChangeLog:
	if test -d .git || test -d ../.git; then		\
	  $(top_srcdir)/build-aux/gitlog-to-changelog		\
	    > $(distdir)/cl-t;					\
	  rm -f $(distdir)/ChangeLog;				\
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;		\
	fi

-include $(top_srcdir)/git.mk
