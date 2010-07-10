m4_define([lyd_major], [0])
m4_define([lyd_minor], [0])
m4_define([lyd_micro], [1])

m4_define([lyd_api_version], [lyd_major.lyd_minor])
m4_define([lyd_version], [lyd_major.lyd_minor.lyd_micro])

# increase the interface age for each release; if the API changes, set to 0
m4_define([lyd_interface_age], [0])
m4_define([lyd_binary_age], [m4_eval(100 * lyd_minor + lyd_micro)])

AC_PREREQ([2.59])

AC_INIT([lyd], [lyd_version], [http://pippin.gimp.org/git/lyd/])
AC_CONFIG_SRCDIR([lyd/lyd.h])

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.10] foreign)

LYD_MAJOR_VERSION=lyd_major
LYD_MINOR_VERSION=lyd_minor
LYD_MICRO_VERSION=lyd_micro
LYD_API_VERSION=lyd_api_version
LYD_VERSION=lyd_version
AC_SUBST(LYD_MAJOR_VERSION)
AC_SUBST(LYD_MINOR_VERSION)
AC_SUBST(LYD_MICRO_VERSION)
AC_SUBST(LYD_API_VERSION)
AC_SUBST(LYD_VERSION)

m4_define([lt_current], [m4_eval(100 * lyd_minor + lyd_micro - lyd_interface_age)])
m4_define([lt_revision], [lyd_interface_age])
m4_define([lt_age], [m4_eval(lyd_binary_age - lyd_interface_age)])
LYD_LT_CURRENT=lt_current
LYD_LT_REV=lt_revision
LYD_LT_AGE=lt_age
LYD_LT_VERSION="$LYD_LT_CURRENT:$LYD_LT_REV:$LYD_LT_AGE"
LYD_LT_LDFLAGS="-version-info $LYD_LT_VERSION"

AC_SUBST(LYD_LT_VERSION)
AC_SUBST(LYD_LT_LDFLAGS)

dnl ========================================================================

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

m4_define([glib_req_version], [2.20.0])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= glib_req_version gthread-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(SDL, sdl)
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

#PKG_CHECK_MODULES(OSC, liblo)
#AC_SUBST(OSC_CFLAGS)
#AC_SUBST(OSC_LIBS)

#PKG_CHECK_MODULES(AO, ao)
#AC_SUBST(AO_CFLAGS)
#AC_SUBST(AO_LIBS)

dnl ========================================================================

AC_CONFIG_FILES([
        Makefile
        lyd/Makefile
        examples/Makefile
        bin/Makefile
        lyd.pc
])

AC_OUTPUT

dnl ========================================================================

echo ""
echo " lyd $VERSION"
echo " ============"
echo ""
echo " prefix:   ${prefix}"
echo ""
echo " Compiler flags:   $MAINTAINER_CFLAGS"
echo ""
