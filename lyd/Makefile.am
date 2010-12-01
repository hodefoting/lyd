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

lyd.s: lyd-vm.c lyd-ops.inc lyd.c
	gcc lyd.c -S $(CFLAGS) $(SSE_FLAGS) $(AM_CFLAGS)

# please, keep the list sorted alphabetically
liblyd_@LYD_API_VERSION@_la_SOURCES = \
        $(srcdir)/lyd.c \
        $(srcdir)/lyd-compiler.c \
        $(srcdir)/lyd-audio-output.c \
        $(srcdir)/lyd-midi.c \
        $(srcdir)/lyd-patches.c \
        $(builddir)/general-midi.inc \
        $(NULL)

BUILT_SOURCES = $(builddir)/general-midi.inc

EXTRA_DIST = lyd-private.h lyd-vm.c biquad.c general-midi.txt

liblyd_@LYD_API_VERSION@_la_LIBADD = $(ALSA_LIBS) $(JACK_LIBS) -lm -lpthread
liblyd_@LYD_API_VERSION@_la_LDFLAGS = $(ALSA_LT_LDFLAGS)  $(JACK_LT_LDFLAGS)

lydheadersdir = $(includedir)/lyd-@LYD_API_VERSION@/lyd

lydheaders_HEADERS = $(top_srcdir)/lyd/lyd.h 

$(builddir)/general-midi.inc: $(srcdir)/general-midi.txt
	echo "static char *midi_patches[256]={" > $@
	cat $< |sed -e 's/^/"/' -e 's/$$/",/' >> $@
	echo "};" >> $@

CLEANFILES += general-midi.inc
