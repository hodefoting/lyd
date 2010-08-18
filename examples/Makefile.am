AM_LDFLAGS = ../lyd/liblyd-$(LYD_API_VERSION).la 
AM_CFLAGS  = -I$(top_srcdir) $(GLIB_CFLAGS)
CFILES = $(wildcard $(srcdir)/*.c)
bins   = $(subst $(srcdir)/,,$(CFILES:.c=))
EXTRA_DIST = $(wildcard *.c) $(wildcard util/*.[ch])
all-local: $(bins)
clean-local:
	rm -f *.lo $(bins)
%: %.c 
	libtool --mode=link --tag=C $(CC) $(DEFS) -I. -I$(top_builddir) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) ../lyd/liblyd-0.0.la $< -o $@


