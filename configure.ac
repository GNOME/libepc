AC_INIT([libepc], [0.3.2], [mathias@openismus.com])
AM_INIT_AUTOMAKE([-Wall dist-bzip2 subdir-objects])

LT_VERSION_INFO="1:1:1"
AC_SUBST(LT_VERSION_INFO)

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([
  Makefile docs/reference/libepc/Makefile
  libepc-1.0.pc libepc-ui-1.0.pc
])

AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_LIBTOOL

GTK_DOC_CHECK([1.4])

PKG_PROG_PKG_CONFIG([0.14])
PKG_CHECK_MODULES(AVAHI_UI,  [avahi-ui     >= 0.6])
PKG_CHECK_MODULES(LIBEPC,    [avahi-client >= 0.6
                              avahi-glib   >= 0.6
                              glib-2.0     >= 2.12
                              gmodule-2.0  >= 2.0
                              gobject-2.0  >= 2.4
                              gnutls       >= 1.4
                              libsoup-2.2  >= 2.2
                              uuid         >= 1.36])
PKG_CHECK_MODULES(LIBEPC_UI, [gtk+-2.0     >= 2.10])

PKG_CHECK_MODULES(GIO,       [gio-2.0      >= 2.15],
                             [have_gio=yes],
                             [have_gio=no])

if test "yes" == "$have_gio"
then
  LIBEPC_CFLAGS="$LIBEPC_CFLAGS $GIO_CFLAGS"
  LIBEPC_LIBS="$LIBEPC_LIBS $GIO_LIBS"
fi

AM_CONDITIONAL([HAVE_AVAHI_UI_0_6_22],
               [$PKG_CONFIG avahi-ui --atleast-version 0.6.22])

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`

AC_SUBST(AVAHI_UI_CFLAGS)
AC_SUBST(AVAHI_UI_LIBS)

AC_SUBST(LIBEPC_CFLAGS)
AC_SUBST(LIBEPC_LIBS)

AC_SUBST(GLIB_GENMARSHAL)
AC_SUBST(GLIB_MKENUMS)

AC_OUTPUT
