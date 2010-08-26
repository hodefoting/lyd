#! /bin/sh
autoreconf -i .
CFLAGS='-Wall -g -O2 -ffast-math'  ./configure $*

