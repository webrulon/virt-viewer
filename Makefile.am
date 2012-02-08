NULL =

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src man plugin po data icons

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
        m4/libtool.m4				\
        m4/ltoptions.m4				\
        m4/ltsugar.m4				\
        m4/ltversion.m4				\
        m4/lt~obsolete.m4			\
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
