#! /bin/sh
autoreconf -i .
CFLAGS='-Wall -g -O2 -ffast-math'  ./configure $1 $2 $3 $4 $5 $6 $7 $8 $9

