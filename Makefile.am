AM_CFLAGS = $(LIBEPC_CFLAGS) -Wall \
	-DGTK_DISABLE_DEPRECATED \
	-DGDK_DISABLE_DEPRECATED \
	-DG_DISABLE_DEPRECATED

AM_LDFLAGS = $(LIBEPC_LIBS)

lib_LTLIBRARIES = \
	libepc/libeasy-publish-consume.la

noinst_PROGRAMS = \
	tests/test-publisher \
	tests/test-consumer

libepc_libeasy_publish_consume_la_SOURCES = \
	libepc/consumer.c \
	libepc/consumer.h \
	libepc/dispatcher.c \
	libepc/dispatcher.h \
	libepc/publisher.c \
	libepc/publisher.h

tests_test_publisher_LDADD = $(LIBEPC_LIBS) libepc/libeasy-publish-consume.la
tests_test_consumer_LDADD = $(LIBEPC_LIBS) libepc/libeasy-publish-consume.la

-include Makefile.local
