NULL =
CLEANFILES = reference.html
DISTCLEANFILES =
EXTRA_DIST = index.html lyd.css lyd.png lyd.ogg
INCLUDES = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/lyd -I$(top_srcdir)/lyd/core
noinst_PROGRAMS = lyd-reference

all-local: reference.html

reference.html: lyd-reference
	./lyd-reference > reference.html
