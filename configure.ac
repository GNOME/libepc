AC_INIT([libepc], [0.1], [mathias@openismus.com])
AM_INIT_AUTOMAKE([-Wall dist-bzip2 subdir-objects])

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([
  Makefile docs/reference/libepc/Makefile
  libepc.pc libepc-ui.pc])

AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_LIBTOOL

GTK_DOC_CHECK([1.4])

PKG_PROG_PKG_CONFIG([0.14])
PKG_CHECK_MODULES(AVAHI_UI, [avahi-ui >= 0.6])
PKG_CHECK_MODULES(LIBEPC, [avahi-client >= 0.6 avahi-glib >= 0.6 gmodule-2.0 gnutls >= 1.6 libsoup-2.2])
PKG_CHECK_MODULES(LIBEPC_UI, [gtk+-2.0])

AM_CONDITIONAL([HAVE_AVAHI_UI_0_6_22], [$PKG_CONFIG avahi-ui --atleast-version 0.6.22])

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`

AC_SUBST(AVAHI_UI_CFLAGS)
AC_SUBST(AVAHI_UI_LIBS)

AC_SUBST(LIBEPC_CFLAGS)
AC_SUBST(LIBEPC_LIBS)

AC_SUBST(GLIB_GENMARSHAL)
AC_SUBST(GLIB_MKENUMS)

AC_OUTPUT
