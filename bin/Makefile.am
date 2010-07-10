AM_LDFLAGS = \
  ../lyd/liblyd-$(LYD_API_VERSION).la \
  $(SDL_LIBS) #$(OSC_LIBS) $(ALSA_LIBS) 
  #$(AO_LIBS) 

AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) \
  $(SDL_CFLAGS) -DHAVE_SDL 
#$(AO_CFLAGS) -DHAVE_AO 
#  $(ALSA_CFLAGS) -DHAVE_ALSA \
#  $(OSC_CFLAGS) -DHAVE_OSC 

bin_PROGRAMS =  lyd
 
lyd_SOURCES = lyd.c audio.c midi.c osc.c welcome.h
