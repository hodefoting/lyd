#! /bin/sh
autoreconf -i .
CFLAGS='-Wall -g'  ./configure $*

