#! /bin/sh
autoreconf -i .
CFLAGS=-Wall ./configure $1 $2 $3 $4 $5 $6 $7 $8 $9

