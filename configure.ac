m4_define([lyd_major], [0])
m4_define([lyd_minor], [0])
m4_define([lyd_micro], [0])

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

#m4_define([glib_req_version], [2.20.0])
#AC_ARG_WITH(glib, [  --without-glib          build without GLIB support])
#have_glib="no"
#if test "x$with_glib" != "xno"; then
#  PKG_CHECK_MODULES(GLIB, glib-2.0 >= glib_req_version gthread-2.0,
#    have_glib="yes"
#    AC_DEFINE(HAVE_GLIB, 1, [Define to 1 if GLIB is available])
#    ,
#    have_glib="no  (GLIB not found)")
#fi
#AC_SUBST(GLIB_CFLAGS)
#AC_SUBST(GLIB_LIBS)
#AM_CONDITIONAL(HAVE_GLIB, test "$have_glib" = "yes")

#AC_ARG_WITH(sdl, [  --without-sdl           build library without SDL audio output])
#have_sdl="no"
#if test "x$with_sdl" != "xno"; then
#  PKG_CHECK_MODULES(SDL, sdl,
#    have_sdl="yes"
#    AC_DEFINE(HAVE_SDL, 1, [Define to 1 if SDL is available])
#    ,
#    have_sdl="no  (SDL not found)")
#fi
#AC_SUBST(SDL_CFLAGS)
#AC_SUBST(SDL_LIBS)
#AM_CONDITIONAL(HAVE_SDL, test "$have_sdl" = "yes")

AC_ARG_WITH(alsa, [  --without-alsa          build library without ALSA audio output, and lyd binary without midi])
have_alsa="no"
if test "x$with_alsa" != "xno"; then
  PKG_CHECK_MODULES(ALSA, alsa,
    have_alsa="yes"
    AC_DEFINE(HAVE_ALSA, 1, [Define to 1 if alsa is available])
    ,
    have_alsa="no  (ALSA not found)")
fi
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
AM_CONDITIONAL(HAVE_ALSA, test "$have_alsa" = "yes")

AC_ARG_WITH(OSC, [  --without-osc           build lyd binary without OSC (liblo) support])
have_osc="no"
if test "x$with_osc" != "xno"; then
  PKG_CHECK_MODULES(OSC, liblo,
    have_osc="yes"
    AC_DEFINE(HAVE_OSC, 1, [Define to 1 if lo library is available])
    ,
    have_osc="no  (SDL not found)")
fi
AC_SUBST(OSC_CFLAGS)
AC_SUBST(OSC_LIBS)
AM_CONDITIONAL(HAVE_OSC, test "$have_osc" = "yes")

#AC_ARG_WITH(ao, [  --without-ao            build library without AO audio output])
#have_ao="no"
#if test "x$with_ao" != "xno"; then
#  PKG_CHECK_MODULES(AO, ao,
#    have_ao="yes"
#    AC_DEFINE(HAVE_AO, 1, [Define to 1 if ao library is available])
#    ,
#    have_ao="no  (AO not found)")
#fi
#AC_SUBST(AO_CFLAGS)
#AC_SUBST(AO_LIBS)
#AM_CONDITIONAL(HAVE_AO, test "$have_ao" = "yes")


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

AC_MSG_RESULT([
 Building lyd $VERSION with prefix = $prefix

 Optional dependencies:
 
 glib:    $have_glib
 alsa:    $have_alsa
 sdl:     $have_sdl
 ao:      $have_ao
 osc:     $have_osc
]);
