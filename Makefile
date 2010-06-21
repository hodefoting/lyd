PKGS = glib-2.0 gthread-2.0 liblo sdl # ao jack alsa

CFLAGS +=  -DHAVE_SDL -DHAVE_OSC # -DHAVE_JACK -DHAVE_ALSA
CFLAGS +=  -mmmx -msse -ffast-math 
CFLAGS += -O2 -g -Wall
CFLAGS +=  -Icore 

LYD_SOURCES=\
 core/lyd.[ch] \
 core/lyd-private.h \
 core/compiler.c

BIN_SOURCES=\
 $(LYD_SOURCES) \
 audio.c\
 midi.c\
 osc.c\
 lyd.c\
 music.c

GAME_FILES=\
 $(LYD_SOURCES) \
 audio.c \
 game.c \
 music.c

all: lyd ui mini

lyd: $(BIN_SOURCES) 
	gcc $(CFLAGS) `pkg-config --libs --cflags $(PKGS)` -lm $+ -o $@

ui: ui.c
	gcc $(CFLAGS) `pkg-config --libs --cflags liblo clutter-1.0` $+ -o $@

mini: $(LYD_SOURCES) mini.c
	gcc -Icore -DNIH -Wall -Os -g -lm -o $@  $+
	strip $@
clean:
	rm -f lyd game mini ui
