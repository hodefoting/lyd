AM_LDFLAGS = ../lyd/liblyd-$(LYD_API_VERSION).la 
AM_CFLAGS  = -I$(top_srcdir) $(GLIB_CFLAGS)
CFILES = $(wildcard $(srcdir)/*.c)
bins = $(subst $(srcdir)/,,$(CFILES:.c=))

noinst_PROGRAMS = interpolated-param lyd-hi midi-test patches scale sun-audio wave-storm 

EXTRA_DIST = $(wildcard *.c) $(wildcard util/*.[ch])

all-local: $(bins) video
clean-local:
	rm -f *.lo $(bins)
%: %.c 
	@echo "  CCBIN  "$@;$(LIBTOOL) --mode=link $(CC) $(DEFS) -I. -I$(top_builddir) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) ../lyd/liblyd-0.0.la $< -o $@ > /dev/null

if HAVE_SDL
video: video.c
	@echo "  CCBIN  "$@;$(LIBTOOL) --mode=link $(CC) $(DEFS) -I. -I$(top_builddir) $(AM_CPPFLAGS) $(SDL_LIBS) $(SDL_CFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) ../lyd/liblyd-0.0.la $< -o $@ > /dev/null
else
video: video.c
	@echo "  SKIP   "$@
endif


if HAVE_SNDFILE
wave-storm: wave-storm.c
	@echo "  CCBIN  "$@;$(LIBTOOL) --mode=link $(CC) $(DEFS) -I. -I$(top_builddir) $(AM_CPPFLAGS) $(SNDFILE_LIBS) $(SNDFILE_CFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) ../lyd/liblyd-0.0.la $< -o $@ > /dev/null
else
wave-storm: wave-storm.c
	@echo "  SKIP   "$@
endif
