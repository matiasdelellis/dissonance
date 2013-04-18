dnl configure.ac
dnl
dnl Pragha - A Lightweight Music Player for GNU/Linux
dnl

dnl Version information
m4_define([pragha_version_major], [0])
m4_define([pragha_version_minor], [8])
m4_define([pragha_version_micro], [11rc1])
m4_define([pragha_version], [pragha_version_major().pragha_version_minor().pragha_version_micro()])

dnl Initialize autoconf
AC_COPYRIGHT([Copyright (c) 2010-2011
        Matias De lellis <mati86dl@gmail.com>])
AC_INIT([pragha], [pragha_version()], [mati86dl@gmail.com])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Initialize automake
AM_INIT_AUTOMAKE([AC_PACKAGE_TARNAME()], [AC_PACKAGE_VERSION()])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE()

dnl *********************
dnl * Debugging support *
dnl *********************
AC_ARG_ENABLE(debug,[  --enable-debug    Turn on debugging],[
	case "${enableval}" in
	   yes)	debug=true ;
		CFLAGS="$CFLAGS -DDEBUG -D_DEBUG -g -ggdb -O0 -fno-inline -fno-inline-functions" ;;
	    no)	debug=false ;
		CFLAGS="$CFLAGS -DNDEBUG" ;;
	     *)	AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
	esac
],[	debug=false
	CFLAGS="$CFLAGS -DNDEBUG"
])
AC_SUBST(CFLAGS)

dnl check for basic programs
AC_PROG_CC()
AC_PROG_INSTALL()
AC_PROG_LIBTOOL()

dnl Check for i18n support
XDT_I18N([cs de es el fr hu it ja nl no pl pt pt_BR ru sv tr uk zh_CN])

dnl Check for required packages
XDT_CHECK_PACKAGE([ALSA], [alsa], [1.0.15])
XDT_CHECK_PACKAGE([AO], [ao], [0.8.8])
XDT_CHECK_PACKAGE([FLAC], [flac], [1.2.1])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.14.0])
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [2.16.3])
XDT_CHECK_PACKAGE([GTK], [gtk+-2.0], [2.16.0])
XDT_CHECK_PACKAGE([TAGLIB], [taglib], [1.4])
XDT_CHECK_PACKAGE([MAD], [mad], [0.15])
XDT_CHECK_PACKAGE([SQLITE3], [sqlite3], [3.4])
XDT_CHECK_PACKAGE([DBUS], [dbus-1], [0.74])
XDT_CHECK_PACKAGE([DBUS_GLIB], [dbus-glib-1], [0.74])
XDT_CHECK_PACKAGE([LIBNOTIFY], [libnotify], [0.4.4])
XDT_CHECK_PACKAGE([SNDFILE], [sndfile >= 1.0.17], [1.0.17])
XDT_CHECK_PACKAGE([VORBISFILE], [vorbisfile], [1.2.0])
XDT_CHECK_PACKAGE([LIBCURL], [libcurl], [7.18])
XDT_CHECK_PACKAGE([LIBCDIO], [libcdio], [0.78])
XDT_CHECK_PACKAGE([LIBCDIO_PARANOIA], [libcdio_paranoia], [0.78])
XDT_CHECK_PACKAGE([LIBCDDB], [libcddb], [1.2.1])
XDT_CHECK_PACKAGE([LIBMODPLUG], [libmodplug], [0.7.0])
XDT_CHECK_PACKAGE([LIBX11], [x11], 1.0.0)

dnl ***********************************
dnl *** Check for taglib_c          ***
dnl ***********************************
TAGLIBC_LIBS=""
TAGLIBC_CFLAGS=""
AC_CHECK_LIB([tag_c], [taglib_file_new],
[
 AC_CHECK_HEADER([taglib/tag_c.h],
 [
  TAGLIBC_LIBS="-ltag_c"
  AC_DEFINE([HAVE_TAGLIBC], [1], [Define to 1 if tag_c is found])
  ])
], [-lm])
AC_SUBST([TAGLIBC_CFLAGS])
AC_SUBST([TAGLIBC_LIBS]) 

dnl *************************************
dnl * Global keyboard shortcuts support *
dnl *************************************
LIBKEYBINDER_FOUND="no"
XDT_CHECK_OPTIONAL_PACKAGE([LIBKEYBINDER],
                       [keybinder], [0.2.0],
                       [keybinder],
                       [keybinder library], [yes])

dnl *************************************
dnl * Mpris2 support need glib 2.26     *
dnl *************************************
GLIB_2_26_FOUND="no"
XDT_CHECK_OPTIONAL_PACKAGE([GLIB_2_26],
                       [glib-2.0], [2.26.0],
                       [glib-2.0],
                       [glib-2.0 library], [yes])

dnl *************************************
dnl * Check libglyr to search info      *
dnl *************************************
LIBGLYR_FOUND="no"
XDT_CHECK_OPTIONAL_PACKAGE([LIBGLYR],
                       [libglyr], [0.9.0],
                       [libglyr],
                       [libglyr library], [yes])

dnl *************************************
dnl * Check exo to session support      *
dnl *************************************
EXO_FOUND="no"
XDT_CHECK_OPTIONAL_PACKAGE([EXO],
                       [exo-1], [0.6.0],
                       [exo-1],
                       [exo-1 library], [yes])
AC_OUTPUT([
Makefile
po/Makefile.in
src/Makefile
])

case "$debug" in
     "true" )
	AC_MSG_RESULT([Debugging enabled, optimizations disabled.]) ;;
esac

AC_MSG_RESULT([CFLAGS = $CFLAGS]) 

echo
echo "Build Configuration for $PACKAGE version $VERSION"
echo ""
echo "  Prefix: ${prefix}"
echo ""
echo "  Global keyboard shortcuts support (Need keybinder >= 0.2.0).......: $LIBKEYBINDER_FOUND"
echo "  MPRIS2 remote control support (Need glib >= 2.26.0)...............: $GLIB_2_26_FOUND"
echo "  Better session management support (Need exo-1 >= 0.6.0)...........: $EXO_FOUND"
echo "  Search lyrics, artists info and albums art (Need libglyr >= 0.9.0): $LIBGLYR_FOUND"
echo

