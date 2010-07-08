PKGS = glib-2.0 gthread-2.0 liblo sdl alsa # ao jack alsa

CFLAGS +=  -DHAVE_SDL -DHAVE_OSC -DHAVE_ALSA # -DHAVE_JACK -DHAVE_ALSA
CFLAGS +=  -mmmx -msse -ffast-math 
CFLAGS += -O0 -g -Wall
CFLAGS +=  -Icore 

CORE_SOURCES=\
 core/lyd.[ch] \
 core/lyd-private.h \
 core/lyd-compiler.c

BIN_SOURCES=\
 $(CORE_SOURCES) \
 audio.c \
 midi.c  \
 osc.c   \
 lyd.c 

TARGETS = lyd ui music mini.gz

all: $(TARGETS)


music: $(CORE_SOURCES) audio.c music.c
	gcc $(CFLAGS) `pkg-config --libs --cflags $(PKGS)` -lm $+ -o $@

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
	rm -f $(TARGETS) mini
