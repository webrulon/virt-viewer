SUBDIRS = 16x16 22x22 24x24 32x32 48x48 256x256

if OS_WIN32
noinst_DATA = virt-viewer.ico
endif

# http://stackoverflow.com/questions/3236115/which-icon-sizes-should-my-windows-applications-icon-include
ico_sizes = 16 32 48 256
%.ico: $(foreach s,$(ico_sizes),$(s)x$(s)/%.png)
	$(AM_V_GEN)$(ICOTOOL) -c -o $@ $^

EXTRA_DIST = virt-viewer.ico

-include $(top_srcdir)/git.mk
