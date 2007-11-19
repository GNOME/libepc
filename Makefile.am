AM_CFLAGS = $(LIBEPC_CFLAGS) -Wall \
	-DGTK_DISABLE_DEPRECATED \
	-DGDK_DISABLE_DEPRECATED \
	-DG_DISABLE_DEPRECATED

AM_LDFLAGS = $(LIBEPC_LIBS)

lib_LTLIBRARIES = \
	libepc/libepc.la
noinst_PROGRAMS = \
	tests/test-publisher \
	tests/test-consumer \
	$(TESTS)
libepc_include_HEADERS = \
	libepc/consumer.h \
	libepc/dispatcher.h \
	libepc/publisher.h
pkgconfig_DATA = \
	libepc.pc

TESTS = \
	tests/test-dispatcher-001

libepc_libepc_la_SOURCES = \
	libepc/consumer.c \
	libepc/dispatcher.c \
	libepc/publisher.c \
	$(libepc_HEADERS)

tests_test_publisher_LDADD = libepc/libepc.la
tests_test_dispatcher_001_LDADD = libepc/libepc.la

tests_test_consumer_CFLAGS = $(AM_CFLAGS) $(AVAHIUI_CFLAGS)
tests_test_consumer_LDADD = libepc/libepc.la $(AVAHIUI_LIBS)

libepc_includedir = $(includedir)/libepc
pkgconfigdir = $(libdir)/pkgconfig

-include Makefile.local

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
SUBDIRS = docs/reference/libepc
