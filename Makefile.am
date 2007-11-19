AM_CFLAGS = $(LIBEPC_CFLAGS) -Wall \
	-DGTK_DISABLE_DEPRECATED \
	-DGDK_DISABLE_DEPRECATED \
	-DG_DISABLE_DEPRECATED

AM_LDFLAGS = $(LIBEPC_LIBS)

lib_LTLIBRARIES = \
	libepc/libepc.la
noinst_LTLIBRARIES = \
	tests/libepc-test.la
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
	libepc/consumer.h \
	libepc/dispatcher.h \
	libepc/marshal.h \
	libepc/publisher.h
pkgconfig_DATA = \
	libepc.pc

libepc_libepc_la_SOURCES = \
	libepc/consumer.c \
	libepc/dispatcher.c \
	libepc/marshal.c \
	libepc/publisher.c \
	$(libepc_HEADERS)
tests_libepc_test_la_SOURCES = \
	tests/framework.c \
	tests/framework.h

examples_test_publisher_LDADD = libepc/libepc.la
examples_test_consumer_CFLAGS = $(AM_CFLAGS) $(AVAHIUI_CFLAGS)
examples_test_consumer_LDADD = libepc/libepc.la $(AVAHIUI_LIBS)

tests_test_dispatcher_simple_service_LDADD = libepc/libepc.la tests/libepc-test.la
tests_test_dispatcher_multiple_services_LDADD = libepc/libepc.la tests/libepc-test.la
tests_test_dispatcher_subtypes_LDADD = libepc/libepc.la tests/libepc-test.la
tests_test_dispatcher_local_collision_LDADD = libepc/libepc.la tests/libepc-test.la

libepc_includedir = $(includedir)/libepc
pkgconfigdir = $(libdir)/pkgconfig

-include Makefile.local

TESTS_ENVIRONMENT = tests/test-runner.sh
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
SUBDIRS = docs/reference/libepc

libepc/marshal.h: libepc/marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=_epc_marshal > $@
libepc/marshal.c: libepc/marshal.list
	$(GLIB_GENMARSHAL) $< --body --prefix=_epc_marshal > $@

