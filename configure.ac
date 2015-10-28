AC_PREREQ([2.63])
AC_INIT([libepc], [0.4.4], [https://bugzilla.gnome.org/enter_bug.cgi?product=libepc], [libepc], [http://live.gnome.org/libepc])
AM_INIT_AUTOMAKE([1.10.3 -Wall dist-bzip2 no-define subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_ARG_VAR([ACLOCAL_FLAGS], [aclocal flags, e.g. -I <macro dir>])
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

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
LT_VERSION_INFO="3:0:0"
AC_SUBST(LT_VERSION_INFO)

GETTEXT_PACKAGE=libepc
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[The gettext domain name])

AC_PROG_CC
AM_PROG_CC_C_O
LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])
IT_PROG_INTLTOOL([0.35.0])
GTK_DOC_CHECK([1.4])
AM_GLIB_GNU_GETTEXT
PKG_PROG_PKG_CONFIG([0.14])

AC_MSG_CHECKING([for glib-genmarshal and glib-mkenums scripts])
PKG_CHECK_EXISTS([glib-2.0],
                 [GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
                  GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
                  AC_MSG_RESULT([yes])],
                 [AC_MSG_ERROR([glib-2.0 not found])])

AC_SUBST(GLIB_GENMARSHAL)
AC_SUBST(GLIB_MKENUMS)

LIBSOUP_24="libsoup-2.4 >= 2.3"
LIBSOUP_22="libsoup-2.2 >= 2.2"
AC_MSG_CHECKING([for a suitable API version of libsoup])
PKG_CHECK_EXISTS([$LIBSOUP_24],
                 [LIBSOUP_API=2.4
                  LIBSOUP=$LIBSOUP_24],
                 [PKG_CHECK_EXISTS([$LIBSOUP_22],
                                   [LIBSOUP_API=2.2
                                    LIBSOUP=$LIBSOUP_22
                                    AC_DEFINE([HAVE_LIBSOUP22],
                                              [1],
                                              [Define to 1 if you still want to use libsoup 2.2.])],
                                   [AC_MSG_ERROR([no suitable libsoup API version found])])])
AC_MSG_RESULT([$LIBSOUP_API])
AC_SUBST(LIBSOUP_API)

GIO_REQUIREMENT="gio-2.0 >= 2.15.1"
AC_MSG_CHECKING([for gio])
PKG_CHECK_EXISTS([$GIO_REQUIREMENT],
                 [AC_MSG_RESULT([yes])
                  GIO=$GIO_REQUIREMENT],
                 [AC_MSG_RESULT([no])])

PKG_CHECK_MODULES(AVAHI_UI,  [avahi-ui-gtk3     >= 0.6])
PKG_CHECK_MODULES(LIBEPC,    [avahi-client >= 0.6
                              avahi-glib   >= 0.6
                              glib-2.0     >= 2.32
                              gthread-2.0  >= 2.32
                              gmodule-2.0  >= 2.0
                              gobject-2.0  >= 2.4
                              gnutls       >= 1.4
                              uuid         >= 1.36
                              $LIBSOUP
                              $GIO])
PKG_CHECK_MODULES(LIBEPC_UI, [gtk+-3.0     >= 3.0])

AC_SUBST(AVAHI_UI_CFLAGS)
AC_SUBST(AVAHI_UI_LIBS)

AC_SUBST(LIBEPC_CFLAGS)
AC_SUBST(LIBEPC_LIBS)

#AC_CHECK_HEADERS([fcntl.h sys/ioctl.h sys/socket.h])
AC_TYPE_UINT16_T
 
#AC_CHECK_FUNCS([memset setlocale socket strchr])
AC_CHECK_LIB([avahi-ui-gtk3],
             [aui_service_dialog_set_service_type_name],
             [AC_DEFINE([HAVE_SET_SERVICE_TYPE_NAME], [1],
                        [Define to 1 if you have aui_service_dialog_set_service_type_name.])],
             [],
             [${AVAHI_UI_LIBS}])

AC_CONFIG_FILES([
  Makefile
  po/Makefile.in
  docs/reference/libepc/Makefile
  libepc-1.0.pc libepc-ui-1.0.pc
])

AC_OUTPUT
