if HAVE_AVAHI_UI_0_6_22
AVAHI_UI_DEFINES = -DHAVE_AVAHI_UI_0_6_22=1
else
AVAHI_UI_DEFINES =
endif

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
	examples/server-credentials \
	$(TESTS)
TESTS = \
	tests/test-consumer-by-name \
	tests/test-dispatcher-local-collision \
	tests/test-dispatcher-multiple-services \
	tests/test-dispatcher-rename \
	tests/test-dispatcher-reset \
	tests/test-dispatcher-simple-service \
	tests/test-dispatcher-subtypes \
	tests/test-publisher-change-name \
	tests/test-publisher-libsoup-494128 \
	tests/test-service-type

libepc_headers = \
	libepc/consumer.h \
	libepc/dispatcher.h \
	libepc/marshal.h \
	libepc/protocol.h \
	libepc/publisher.h \
	libepc/service-type.h \
	libepc/shell.h \
	libepc/tls.h
libepc_include_HEADERS = \
	$(libepc_headers) \
	libepc/enums.h
libepc_ui_include_HEADERS = \
	libepc-ui/entropy-window.h \
	libepc-ui/password-dialog.h
pkgconfig_DATA = \
	libepc.pc \
	libepc-ui.pc

libepc_libepc_la_SOURCES = \
	$(libepc_include_HEADERS) \
	libepc/consumer.c \
	libepc/dispatcher.c \
	libepc/enums.c \
	libepc/marshal.c \
	libepc/protocol.c \
	libepc/publisher.c \
	libepc/service-type.c \
	libepc/shell.c \
	libepc/tls.c

libepc_ui_libepc_ui_la_SOURCES = \
	$(libepc_ui_include_HEADERS) \
	libepc-ui/entropy-window.c \
	libepc-ui/password-dialog.c

tests_libepc_tests_la_SOURCES = \
	tests/framework.c \
	tests/framework.h

libepc_libepc_la_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS) -DGETTEXT_PACKAGE=\"libepc\"
libepc_libepc_la_LDFLAGS = $(LIBEPC_LIBS)

libepc_ui_libepc_ui_la_CFLAGS = $(AM_CFLAGS) $(LIBEPC_UI_CFLAGS) -DGETTEXT_PACKAGE=\"libepc-ui\"
libepc_ui_libepc_ui_la_LDFLAGS = $(LIBEPC_UI_LIBS) libepc/libepc.la

examples_test_publisher_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
examples_test_publisher_LDADD = libepc/libepc.la
examples_test_consumer_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS) $(LIBEPC_UI_CFLAGS) $(AVAHI_UI_CFLAGS) $(AVAHI_UI_DEFINES)
examples_test_consumer_LDADD = libepc-ui/libepc-ui.la $(AVAHI_UI_LIBS)
examples_server_credentials_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS) $(LIBEPC_UI_CFLAGS)
examples_server_credentials_LDADD = libepc/libepc.la libepc-ui/libepc-ui.la

tests_libepc_tests_la_CFLAGS = $(AM_CFLAGS) $(LIBEPC_CFLAGS)

tests_test_consumer_by_name_CFLAGS 		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_consumer_by_name_LDADD 		= libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_local_collision_CFLAGS	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_local_collision_LDADD 	= libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_multiple_services_CFLAGS 	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_multiple_services_LDADD 	= libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_rename_CFLAGS 		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_rename_LDADD 		= libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_reset_CFLAGS 		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_reset_LDADD 		= libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_simple_service_CFLAGS 	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_simple_service_LDADD 	= libepc/libepc.la tests/libepc-tests.la
tests_test_dispatcher_subtypes_CFLAGS 		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_subtypes_LDADD 		= libepc/libepc.la tests/libepc-tests.la
tests_test_publisher_change_name_CFLAGS      	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_publisher_change_name_LDADD 		= libepc/libepc.la tests/libepc-tests.la
tests_test_publisher_libsoup_494128_CFLAGS      = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_publisher_libsoup_494128_LDADD 	= libepc/libepc.la tests/libepc-tests.la
tests_test_service_type_CFLAGS      		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_service_type_LDADD 			= libepc/libepc.la tests/libepc-tests.la

libepc_includedir = $(includedir)/libepc/libepc
libepc_ui_includedir = $(includedir)/libepc/libepc-ui
pkgconfigdir = $(libdir)/pkgconfig

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
TESTS_ENVIRONMENT = tests/test-runner.sh

all-local:
	cd docs/reference/libepc && $(MAKE) $(AM_MAKEFLAGS) all
clean-local:
	cd docs/reference/libepc && $(MAKE) $(AM_MAKEFLAGS) clean
dist-hook:
	cd docs/reference/libepc && $(MAKE) $(AM_MAKEFLAGS) dist
docs:
	cd docs/reference/libepc && $(MAKE) $(AM_MAKEFLAGS) docs
install-data-local:
	cd docs/reference/libepc && $(MAKE) $(AM_MAKEFLAGS) install-data
maintainer-clean-local:
	cd docs/reference/libepc && $(MAKE) $(AM_MAKEFLAGS) maintainer-clean
uninstall-local:
	cd docs/reference/libepc && $(MAKE) $(AM_MAKEFLAGS) uninstall

libepc/marshal.h: libepc/marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=_epc_marshal > $@ || { rm -f $@; false; }
libepc/marshal.c: libepc/marshal.list
	( echo '#include "marshal.h"' && $(GLIB_GENMARSHAL) $< --body --prefix=_epc_marshal ) > $@ || { rm -f $@; false; }
libepc/enums.h.tmp: libepc/enums.h.in $(libepc_headers)
	$(GLIB_MKENUMS) --template $< $(libepc_headers) > $@ || { rm -f $@; false; }
libepc/enums.h.stamp: libepc/enums.h.tmp
	cmp -s $< libepc/enums.h || cp $< libepc/enums.h; touch $@
libepc/enums.h: libepc/enums.h.stamp

libepc/enums.c.tmp: libepc/enums.c.in $(libepc_headers)
	$(GLIB_MKENUMS) --template $< $(libepc_headers) > $@ || { rm -f $@; false; }
libepc/enums.c.stamp: libepc/enums.c.tmp
	cmp -s $< libepc/enums.c || cp $< libepc/enums.c; touch $@
libepc/enums.c: libepc/enums.c.stamp

-include Makefile.local
