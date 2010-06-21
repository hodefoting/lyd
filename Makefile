PKGS = glib-2.0 gthread-2.0 ao liblo jack sdl alsa

CFLAGS = "-O2 -g -Wall"
CFLAGS += " -DHAVE_ALSA"

LYD_SOURCES=\
 lyd.[ch] \
 lyd-private.h 

BIN_SOURCES=\
 $(LYD_SOURCES) \
 audio.c\
 compiler.c\
 midi.c\
 osc.c\
 test.c\
 music.c

GAME_FILES=\
 $(LYD_SOURCES) \
 audio.c \
 game.c \
 music.c

all: lyd 


pcm: $(BIN_SOURCES)
	gcc -mmmx -msse -ffast-math -Wall -O2 -g `pkg-config --libs --cflags $(PKGS)` -lm pcm.c -o $@

lyd: $(BIN_SOURCES)
	gcc -mmmx -msse -ffast-math -Wall -g `pkg-config --libs --cflags $(PKGS)` -lm $(BIN_SOURCES) -o $@

game: $(GAME_FILES)
	gcc -Wall -O2 -g -o $@ `pkg-config --libs --cflags $(PKGS) clutter-1.0` -lm $(GAME_FILES)
ui: ui.c
	gcc -Wall -O2 -g -o $@ `pkg-config --libs --cflags liblo clutter-1.0` ui.c
run: lyd
	./lyd

mini: lyd.c lyd.h mini.c compiler.c
	gcc -Wall -Os -g -lm -o $@  $?
	

clean:
	rm -f lyd game
