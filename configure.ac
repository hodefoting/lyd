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

AM_INIT_AUTOMAKE([1.9] foreign)

dnl ----------------------------- from acbirnet.m4 -------------------------------
dnl Check whether cc accepts a certain option
dnl MC_PROG_CC_SUPPORTS_OPTION(OPTIONS, ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND])
AC_DEFUN([MC_PROG_CC_SUPPORTS_OPTION], [
AC_MSG_CHECKING([whether ${CC-cc} supports $1])
echo >conftest.c;
if ${CC-cc} [$1] -c $CFLAGS conftest.c >/dev/null 2>&1 ; then
    AC_MSG_RESULT(yes)
    [$2]
else
    AC_MSG_RESULT(no)
    [$3]
fi
rm -fr conftest*
])dnl


#MC_PROG_CC_SUPPORTS_OPTION([-msse -msse2 -msse3 -mmmx -ftree-vectorize -ffastmath -mtune=native -march=native], [
#   SSE_FLAGS="-msse3 -msse2 -msse -mmmx -ftree-vectorize -O3 -ffastmath -mtune=native -march=native"
MC_PROG_CC_SUPPORTS_OPTION([-msse -mmmx -O3 -ftree-vectorize -ffast-math], [
   SSE_FLAGS="-msse -mmmx -O3 -ftree-vectorize  -ffast-math " # -ftree-vectorizer-verbose=3"
],[
   SSE_FLAGS=""
])
AC_SUBST(SSE_FLAGS)


#m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

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
AC_PROG_LIBTOOL

AC_CHECK_LIB([pthread], [pthread_create])

AC_ARG_WITH(alsa, [  --without-alsa          build library without ALSA audio output, and lyd binary without midi])
have_alsa="no"
if test "x$with_alsa" != "xno"; then
  PKG_CHECK_MODULES(ALSA, alsa,
    have_alsa="yes"
    AC_DEFINE(HAVE_ALSA, 1, [Define to 1 if alsa is available])
    ,
    have_alsa="no  (ALSA not found, you need at least one audio driver)")
fi
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
AM_CONDITIONAL(HAVE_ALSA, test "$have_alsa" = "yes")

AC_ARG_WITH(sdl, [  --without-sdl          build library without SDL support])
have_sdl="no"
if test "x$with_sdl" != "xno"; then
  PKG_CHECK_MODULES(SDL, sdl,
    have_sdl="yes"
    AC_DEFINE(HAVE_SDL, 1, [Define to 1 if sdl is available])
    ,
    have_sdl="no  (sdl not found)")
fi
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)
AM_CONDITIONAL(HAVE_SDL, test "$have_sdl" = "yes")

AC_ARG_WITH(sndfile, [  --without-sndfile          build library without SNDFILE audio output, and lyd binary without midi])
have_sndfile="no"
if test "x$with_sndfile" != "xno"; then
  PKG_CHECK_MODULES(SNDFILE, sndfile,
    have_sndfile="yes"
    AC_DEFINE(HAVE_SNDFILE, 1, [Define to 1 if sndfile is available])
    ,
    have_sndfile="no  (sndfile not found)")
fi
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)
AM_CONDITIONAL(HAVE_SNDFILE, test "$have_sndfile" = "yes")


AC_ARG_WITH(jack, [  --without-jack          build library without JACK audio output, and lyd binary without midi])
have_jack="no"
if test "x$with_jack" != "xno"; then
  PKG_CHECK_MODULES(JACK, jack,
    have_jack="yes"
    AC_DEFINE(HAVE_JACK, 1, [Define to 1 if jack is available])
    ,
    have_jack="no")
fi
AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)
AM_CONDITIONAL(HAVE_JACK, test "$have_jack" = "yes")

AC_ARG_WITH(OSC, [  --without-osc           build lyd binary without OSC (liblo) support])
have_osc="no"
if test "x$with_osc" != "xno"; then
  PKG_CHECK_MODULES(OSC, liblo,
    have_osc="yes"
    AC_DEFINE(HAVE_OSC, 1, [Define to 1 if lo library is available])
    ,
    have_osc="no  (liblo not found)")
fi
AC_SUBST(OSC_CFLAGS)
AC_SUBST(OSC_LIBS)
AM_CONDITIONAL(HAVE_OSC, test "$have_osc" = "yes")

dnl ========================================================================

AC_ARG_WITH(MMM, [  --without-mmm           build without mmm audio output support])
have_mmm="no"
if test "x$with_mmm" != "xno"; then
  PKG_CHECK_MODULES(MMM, mmm,
    have_mmm="yes"
    AC_DEFINE(HAVE_MMM, 1, [Define to 1 if mmm library is available])
    ,
    have_mmm="no  (libmmm not found)")
fi
AC_SUBST(MMM_CFLAGS)
AC_SUBST(MMM_LIBS)
AM_CONDITIONAL(HAVE_MMM, test "$have_mmm" = "yes")

dnl ========================================================================


AC_CHECK_HEADERS(dl.h)
AC_SEARCH_LIBS([dlopen], [dl])
AC_SEARCH_LIBS([sin],    [m])

dnl ========================================================================

AC_CONFIG_FILES([
   Makefile
   lyd/Makefile
   examples/Makefile
   bin/Makefile
   doc/Makefile
   lyd.pc
])

AC_OUTPUT

dnl ========================================================================

AC_MSG_RESULT([
 Building lyd $VERSION with prefix = $prefix

 Optional dependencies:
 
  alsa:     $have_alsa
  jack:     $have_jack
  mmm:      $have_mmm
  sndfile:  $have_sndfile
  osc:      $have_osc
]);

