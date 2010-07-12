NULL =

CLEANFILES =
DISTCLEANFILES =

INCLUDES = -I$(srcdir) -I$(top_srcdir) 

AM_CPPFLAGS = \
        -DPREFIX=\""$(prefix)"\"  \
        $(NULL)

AM_CFLAGS = $(MAINTAINER_CFLAGS) $(GLIB_CFLAGS) $(SDL_CFLAGS) $(AO_CFLAGS) $(JACK_CFLAGS)

lib_LTLIBRARIES = liblyd-@LYD_API_VERSION@.la

# please, keep the list sorted alphabetically
liblyd_@LYD_API_VERSION@_la_SOURCES = \
        $(srcdir)/lyd.c \
        $(srcdir)/lyd-compiler.c \
        $(srcdir)/lyd-audio-output.c \
        $(srcdir)/lyd-midi.c \
        $(NULL)

EXTRA_DIST = lyd-private.h lyd-voice.c biquad.c

liblyd_@LYD_API_VERSION@_la_LIBADD = $(GLIB_LIBS) $(SDL_LIBS) $(AO_LIBS) $(JACK_CFLAGS) -lm
liblyd_@LYD_API_VERSION@_la_LDFLAGS = $(GLIB_LT_LDFLAGS) $(SDL_LT_LDFLAGS) $(AO_LT_LDFLAGS) $(JACK_LT_LDFLAGS)

lydheadersdir = $(includedir)/lyd-@LYD_API_VERSION@/lyd

lydheaders_HEADERS = $(top_srcdir)/lyd/lyd.h 

