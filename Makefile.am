SUBDIRS = lyd bin examples

lyd-@LYD_API_VERSION@.pc: lyd.pc
	$(QUIET_GEN)cp -f lyd.pc lyd-$(LYD_API_VERSION).pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lyd-$(LYD_API_VERSION).pc

CLEANFILES = lyd-$(LYD_API_VERSION).pc
DISTCLEANFILES = lyd.pc
EXTRA_DIST = lyd.pc.in
