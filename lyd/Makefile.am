NULL =

CLEANFILES =
DISTCLEANFILES =

INCLUDES = -I$(srcdir) -I$(top_srcdir)

AM_CPPFLAGS = \
        -DPREFIX=\""$(prefix)"\"  \
	-I$(builddir) $(SSE_FLAGS)
        $(NULL)

AM_CFLAGS = $(MAINTAINER_CFLAGS) $(JACK_CFLAGS) -DNIH

lib_LTLIBRARIES = liblyd-@LYD_API_VERSION@.la

lyd-vm.s: core/lyd-vm.c core/lyd-ops.inc
	gcc core/lyd-vm.c -S $(CFLAGS) $(INCLUDES) $(SSE_FLAGS) $(AM_CFLAGS)

# please, keep the list sorted alphabetically
liblyd_@LYD_API_VERSION@_la_SOURCES = \
        $(srcdir)/lyd-audio-output.c \
        $(srcdir)/lyd-patches.c \
        $(builddir)/general-midi.inc \
        $(srcdir)/lyd-midi.c \
        $(NULL)

EXTRA_DIST = core/lyd-private.h core/lyd-ops.inc core/lyd-ops.c general-midi.txt

liblyd_@LYD_API_VERSION@_la_LIBADD = liblyd-core-@LYD_API_VERSION@.la \
                                     $(ALSA_LIBS) $(JACK_LIBS) -lm -lpthread
liblyd_@LYD_API_VERSION@_la_LDFLAGS = $(ALSA_LT_LDFLAGS)  $(JACK_LT_LDFLAGS)

lydheadersdir = $(includedir)/lyd-@LYD_API_VERSION@/lyd

lydheaders_HEADERS = $(top_srcdir)/lyd/lyd.h $(top_srcdir)/lyd/core/lyd-extend.h

BUILT_SOURCES = $(builddir)/general-midi.inc

$(builddir)/general-midi.inc: $(srcdir)/general-midi.txt
	echo "static char *midi_patches[256]={" > $@
	cat $< |sed -e 's/^/"/' -e 's/$$/",/' >> $@
	echo "};" >> $@

CLEANFILES += general-midi.inc

noinst_LTLIBRARIES = liblyd-core-@LYD_API_VERSION@.la

EXTRA_DIST += core/lyd-private.h core/biquad.c

# please, keep the list sorted alphabetically
liblyd_core_@LYD_API_VERSION@_la_SOURCES = \
        $(srcdir)/core/lyd.c \
        $(srcdir)/core/lyd-mixer.c \
        $(srcdir)/core/lyd-vm.c \
        $(srcdir)/core/lyd-compiler.c \
        $(NULL)

liblyd_core_@LYD_API_VERSION@_la_LIBADD = -lm
liblyd_core_@LYD_API_VERSION@_la_LDFLAGS =
