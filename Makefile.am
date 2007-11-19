AM_CFLAGS = \
	-Wall \
	-DGTK_DISABLE_DEPRECATED=1 \
	-DGDK_DISABLE_DEPRECATED=1 \
	-DG_DISABLE_DEPRECATED=1

lib_LTLIBRARIES = \
	libepc/libepc.la \
	libepc-ui/libepc-ui.la
noinst_LTLIBRARIES = \
	tests/libepc-tests.la
noinst_PROGRAMS = \
	examples/test-publisher \
	examples/test-consumer \
	$(TESTS)
TESTS = \
	tests/test-dispatcher-simple-service \
	tests/test-dispatcher-multiple-services \
	tests/test-dispatcher-subtypes \
	tests/test-dispatcher-local-collision
libepc_include_HEADERS = \
	libepc/avahi-shell.c \
	libepc/consumer.h \
	libepc/dispatcher.h \
	libepc/marshal.h \
	libepc/publisher.h
libepc_ui_include_HEADERS = \
	libepc-ui/passworddialog.h
pkgconfig_DATA = \
	libepc.pc \
	libepc-ui.pc

libepc_libepc_la_SOURCES = \
	libepc/avahi-shell.c \
	libepc/consumer.c \
	libepc/dispatcher.c \
	libepc/marshal.c \
	libepc/publisher.c \
	$(libepc_HEADERS)

libepc_ui_libepc_ui_la_SOURCES = \
	libepc-ui/passworddialog.c \
	$(libepc_ui_HEADERS)

tests_libepc_tests_la_SOURCES = \
	tests/framework.c \
	tests/framework.h

libepc_libepc_la_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS) -DGETTEXT_PACKAGE=\"libepc\"
libepc_libepc_la_LDFLAGS = $(LIBEPC_LIBS)

libepc_ui_libepc_ui_la_CFLAGS = $(AM_CFLAGS) $(LIBEPC_UI_CFLAGS) -DGETTEXT_PACKAGE=\"libepc-ui\"
libepc_ui_libepc_ui_la_LDFLAGS = $(LIBEPC_UI_LIBS)

examples_test_publisher_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
examples_test_publisher_LDADD = libepc/libepc.la
examples_test_consumer_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS) $(LIBEPC_UI_CFLAGS) $(AVAHIUI_CFLAGS)
examples_test_consumer_LDADD = libepc/libepc.la libepc-ui/libepc-ui.la $(AVAHIUI_LIBS)

tests_libepc_tests_la_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_simple_service_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_simple_service_LDADD = libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_multiple_services_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_multiple_services_LDADD = libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_subtypes_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_subtypes_LDADD = libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_local_collision_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_local_collision_LDADD = libepc/libepc.la tests/libepc-tests.la

libepc_includedir = $(includedir)/libepc/libepc
libepc_ui_includedir = $(includedir)/libepc/libepc-ui
pkgconfigdir = $(libdir)/pkgconfig

-include Makefile.local

TESTS_ENVIRONMENT = tests/test-runner.sh
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
SUBDIRS = docs/reference/libepc

libepc/marshal.h: libepc/marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=_epc_marshal > $@
libepc/marshal.c: libepc/marshal.list
	$(GLIB_GENMARSHAL) $< --body --prefix=_epc_marshal > $@

