AM_LDFLAGS = \
  ../lyd/liblyd-$(LYD_API_VERSION).la $(OSC_LIBS) $(ALSA_LIBS) $(SNDFILE_LIBS)

AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(OSC_CFLAGS) $(ALSA_CFLAGS) $(SNDFILE_CFLAGS)

bin_PROGRAMS =  lyd
lyd_SOURCES = lyd.c osc.c welcome.h
