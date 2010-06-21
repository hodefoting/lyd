PKGS = glib-2.0 gthread-2.0 liblo sdl # ao jack alsa

CFLAGS +=  -DHAVE_SDL -DHAVE_OSC # -DHAVE_JACK -DHAVE_ALSA
CFLAGS +=  -mmmx -msse -ffast-math 
CFLAGS += -O2 -g -Wall
CFLAGS +=  -Icore 

CORE_SOURCES=\
 core/lyd.[ch] \
 core/lyd-private.h \
 core/compiler.c

BIN_SOURCES=\
 $(CORE_SOURCES) \
 audio.c \
 midi.c  \
 osc.c   \
 lyd.c   \
 music.c

all: lyd ui mini.gz

lyd: $(BIN_SOURCES) 
	gcc $(CFLAGS) `pkg-config --libs --cflags $(PKGS)` -lm $+ -o $@

ui: ui.c
	gcc $(CFLAGS) `pkg-config --libs --cflags liblo clutter-1.0` $+ -o $@

# a minimal target to see how small the compiled core can be
mini: $(CORE_SOURCES) mini.c
	gcc -Icore -DNIH -Wall -Os -g -lm -o $@  $+
mini.gz: mini
	strip -s $<
	gzip -f $<
	touch mini
	wc -c mini.gz
	wc -l core/*.[ch]
clean:
	rm -f lyd game mini ui
