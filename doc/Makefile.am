NULL =
CLEANFILES = reference.html
DISTCLEANFILES =
INCLUDES = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/lyd
noinst_PROGRAMS = lyd-reference

all-local: reference.html

reference.html: lyd-reference
	./lyd-reference > reference.html
