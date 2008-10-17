AC_INIT([libepc], [0.3.6], [mathias@openismus.com])
AM_INIT_AUTOMAKE([-Wall dist-bzip2 subdir-objects])

GNOME_COMMON_INIT

dnl LT_VERSION_INFO=CURRENT:REVISION:AGE
dnl
dnl Updating that variable is neccessary, but complicated.
dnl Therefore some rules, taken from the libtool manual.
dnl
dnl 1. Update only immediately before a public release.
dnl 2. If the library source code has changed at all since the last
dnl    update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
dnl 3. If any interfaces have been added, removed, or changed since
dnl    the last update, increment CURRENT, and set REVISION to 0.
dnl 4. If any interfaces have been added, then increment AGE.
dnl 5. If any interfaces have been removed, then set AGE to 0.
dnl
LT_VERSION_INFO="1:2:0"
AC_SUBST(LT_VERSION_INFO)

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([
  Makefile
  po/Makefile.in
  docs/reference/libepc/Makefile
  libepc-1.0.pc libepc-ui-1.0.pc
])

GETTEXT_PACKAGE=libepc
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[The gettext domain name])

AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.35.0])
GTK_DOC_CHECK([1.4])
AM_GLIB_GNU_GETTEXT

PKG_PROG_PKG_CONFIG([0.14])
PKG_CHECK_MODULES(AVAHI_UI,  [avahi-ui     >= 0.6])
PKG_CHECK_MODULES(LIBEPC,    [avahi-client >= 0.6
                              avahi-glib   >= 0.6
                              glib-2.0     >= 2.12
                              gthread-2.0  >= 2.12
                              gmodule-2.0  >= 2.0
                              gobject-2.0  >= 2.4
                              gnutls       >= 1.4
                              uuid         >= 1.36])
PKG_CHECK_MODULES(LIBEPC_UI, [gtk+-2.0     >= 2.10])

PKG_CHECK_MODULES(LIBSOUP,   [libsoup-2.4  >= 2.3],
                             [LIBSOUP_API=2.4],[
PKG_CHECK_MODULES(LIBSOUP,   [libsoup-2.2  >= 2.2],
                             [LIBSOUP_API=2.2])])

PKG_CHECK_MODULES(GIO,       [gio-2.0      >= 2.15.1],
                             [have_gio=yes],
                             [have_gio=no])

if test "2.2" == "$LIBSOUP_API"
then
  AC_DEFINE([HAVE_LIBSOUP22], [1], [Define to 1 if you still want to use libsoup 2.2.])
fi

if test "yes" == "$have_gio"
then
  LIBEPC_CFLAGS="$LIBEPC_CFLAGS $GIO_CFLAGS"
  LIBEPC_LIBS="$LIBEPC_LIBS $GIO_LIBS"
fi

LIBEPC_CFLAGS="$LIBEPC_CFLAGS $LIBSOUP_CFLAGS"
LIBEPC_LIBS="$LIBEPC_LIBS $LIBSOUP_LIBS"

AC_CHECK_LIB([avahi-ui],
             [aui_service_dialog_set_service_type_name],
             [AC_DEFINE([HAVE_SET_SERVICE_TYPE_NAME], [1],
                        [Define to 1 if you have aui_service_dialog_set_service_type_name.])],
             [],
             [${AVAHI_UI_LIBS}])

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`

AC_SUBST(AVAHI_UI_CFLAGS)
AC_SUBST(AVAHI_UI_LIBS)

AC_SUBST(LIBEPC_CFLAGS)
AC_SUBST(LIBEPC_LIBS)

AC_SUBST(GLIB_GENMARSHAL)
AC_SUBST(GLIB_MKENUMS)

AC_SUBST(LIBSOUP_API)

AC_OUTPUT

AC_MSG_NOTICE([============================================])
AC_MSG_NOTICE([  libsoup API ........................ $LIBSOUP_API])
AC_MSG_NOTICE([  gio support ........................ $have_gio])
AC_MSG_NOTICE([============================================])
