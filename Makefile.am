# ===========================
# -- CONFIGURATION OPTIONS --
# ===========================

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
TESTS_ENVIRONMENT = $(top_srcdir)/tests/test-runner.sh

# ============================
# -- DIRECTORY DECLARATIONS --
# ============================

libepc_includedir = $(includedir)/libepc-1.0/libepc
libepc_ui_includedir = $(includedir)/libepc-ui-1.0/libepc-ui
pkgconfigdir = $(libdir)/pkgconfig

gtkdoc_builddir=$(top_builddir)/docs/reference/libepc
gtkdoc_distdir=$(top_distdir)/docs/reference/libepc
gtkdoc_srcdir=$(top_srcdir)/docs/reference/libepc

# ==================
# -- TARGET LISTS --
# ==================

lib_LTLIBRARIES = \
	libepc/libepc-1.0.la \
	libepc-ui/libepc-ui-1.0.la
noinst_LTLIBRARIES = \
	tests/libepc-tests.la
noinst_PROGRAMS = \
	examples/list-resources \
	examples/lookup-resource \
	examples/service-browser \
	examples/simple-publisher \
	examples/consumer-ui \
	examples/publisher-ui \
	examples/server-credentials \
	$(TESTS)
TESTS = \
	tests/test-consumer-by-info \
	tests/test-consumer-by-name \
	tests/test-dispatcher-local-collision \
	tests/test-dispatcher-multiple-services \
	tests/test-dispatcher-rename \
	tests/test-dispatcher-reset \
	tests/test-dispatcher-simple-service \
	tests/test-dispatcher-subtypes \
	tests/test-expand-name \
	tests/test-progress-hooks \
	tests/test-publisher-bookmarks \
	tests/test-publisher-change-name \
	tests/test-publisher-libsoup-494128 \
	tests/test-publisher-unique \
	tests/test-service-type

# ================
# -- FILE LISTS --
# ================

libepc_headers = \
	libepc/consumer.h \
	libepc/contents.h \
	libepc/dispatcher.h \
	libepc/marshal.h \
	libepc/protocol.h \
	libepc/publisher.h \
	libepc/service-info.h \
	libepc/service-monitor.h \
	libepc/service-type.h \
	libepc/shell.h \
	libepc/tls.h
libepc_include_HEADERS = \
	$(libepc_headers) \
	libepc/enums.h \
	libepc/marshal.h
libepc_ui_include_HEADERS = \
	libepc-ui/password-dialog.h \
	libepc-ui/progress-window.h
pkgconfig_DATA = \
	libepc-1.0.pc \
	libepc-ui-1.0.pc
EXTRA_DIST = \
	gtk-doc.make \
	$(gtkdoc_srcdir)/libepc-1.0-docs.xml \
	$(gtkdoc_srcdir)/libepc-1.0-sections.txt \
	tests/test-runner.sh

BUILT_SOURCES = \
	libepc/enums.c \
	libepc/enums.h \
	libepc/marshal.c \
	libepc/marshal.h
libepc_libepc_1_0_la_SOURCES = \
	$(libepc_include_HEADERS) \
	libepc/consumer.c \
	libepc/contents.c \
	libepc/dispatcher.c \
	libepc/enums.c \
	libepc/enums.c.in \
	libepc/enums.h.in \
	libepc/marshal.c \
	libepc/marshal.list \
	libepc/protocol.c \
	libepc/publisher.c \
	libepc/service-info.c \
	libepc/service-monitor.c \
	libepc/service-type.c \
	libepc/shell.c \
	libepc/tls.c
libepc_ui_libepc_ui_1_0_la_SOURCES = \
	$(libepc_ui_include_HEADERS) \
	libepc-ui/password-dialog.c \
	libepc-ui/progress-window.c
tests_libepc_tests_la_SOURCES = \
	tests/framework.c \
	tests/framework.h

# ====================
# -- COMPILER FLAGS --
# ====================

AM_CFLAGS = \
	-Wall \
	-DG_DISABLE_DEPRECATED=1 \
	-DGDK_DISABLE_DEPRECATED=1 \
	-DGTK_DISABLE_DEPRECATED=1

if HAVE_AVAHI_UI_0_6_22
AVAHI_UI_DEFINES = -DHAVE_AVAHI_UI_0_6_22=1
else
AVAHI_UI_DEFINES =
endif

LIBEPC_UI_CFLAGS += $(AVAHI_UI_DEFINES)

libepc_libepc_1_0_la_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS) -DGETTEXT_PACKAGE=\"libepc\" -DG_LOG_DOMAIN=\"libepc\"
libepc_libepc_1_0_la_LDFLAGS			= $(LIBEPC_LIBS) -version-info $(LT_VERSION_INFO)

libepc_ui_libepc_ui_1_0_la_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_UI_CFLAGS) -DGETTEXT_PACKAGE=\"libepc-ui\" -DG_LOG_DOMAIN=\"libepc-ui\"
libepc_ui_libepc_ui_1_0_la_LDFLAGS		= $(LIBEPC_UI_LIBS) -version-info $(LT_VERSION_INFO) libepc/libepc-1.0.la

examples_list_resources_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
examples_list_resources_LDADD			= libepc/libepc-1.0.la
examples_lookup_resource_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
examples_lookup_resource_LDADD			= libepc/libepc-1.0.la
examples_service_browser_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
examples_service_browser_LDADD			= libepc/libepc-1.0.la
examples_simple_publisher_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
examples_simple_publisher_LDADD			= libepc/libepc-1.0.la

examples_consumer_ui_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS) $(LIBEPC_UI_CFLAGS) $(AVAHI_UI_CFLAGS)
examples_consumer_ui_LDADD			= libepc/libepc-1.0.la libepc-ui/libepc-ui-1.0.la $(AVAHI_UI_LIBS)
examples_publisher_ui_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS) $(LIBEPC_UI_CFLAGS) $(AVAHI_UI_CFLAGS) -rdynamic
examples_publisher_ui_LDADD			= libepc/libepc-1.0.la libepc-ui/libepc-ui-1.0.la $(AVAHI_UI_LIBS)
examples_server_credentials_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS) $(LIBEPC_UI_CFLAGS)
examples_server_credentials_LDADD		= libepc/libepc-1.0.la libepc-ui/libepc-ui-1.0.la

tests_libepc_tests_la_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS)

tests_test_consumer_by_info_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_consumer_by_info_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_consumer_by_name_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_consumer_by_name_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_dispatcher_local_collision_CFLAGS	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_local_collision_LDADD	= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_dispatcher_multiple_services_CFLAGS	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_multiple_services_LDADD	= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_dispatcher_rename_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_rename_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_dispatcher_reset_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_reset_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_dispatcher_simple_service_CFLAGS	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_simple_service_LDADD	= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_dispatcher_subtypes_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_dispatcher_subtypes_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_expand_name_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_expand_name_LDADD			= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_progress_hooks_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS) $(LIBEPC_UI_CFLAGS)
tests_test_progress_hooks_LDADD			= libepc/libepc-1.0.la libepc-ui/libepc-ui-1.0.la tests/libepc-tests.la
tests_test_publisher_bookmarks_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_publisher_bookmarks_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_publisher_change_name_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_publisher_change_name_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_publisher_libsoup_494128_CFLAGS	= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_publisher_libsoup_494128_LDADD	= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_publisher_unique_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_publisher_unique_LDADD		= libepc/libepc-1.0.la tests/libepc-tests.la
tests_test_service_type_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS)
tests_test_service_type_LDADD			= libepc/libepc-1.0.la tests/libepc-tests.la

# ==================
# -- CUSTOM RULES --
# ==================

libepc/marshal.h: libepc/marshal.list
	$(GLIB_GENMARSHAL) $< --header --prefix=_epc_marshal > $@ || { rm -f $@; false; }
libepc/marshal.c: libepc/marshal.list
	( echo '#include "marshal.h"' && $(GLIB_GENMARSHAL) $< --body --prefix=_epc_marshal ) > $@ || { rm -f $@; false; }
libepc/enums.h.tmp: libepc/enums.h.in $(libepc_headers)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template $< $(libepc_headers)) > $@ || { rm -f $@; false; }
libepc/enums.h.stamp: libepc/enums.h.tmp
	cmp -s $< libepc/enums.h || cp $< libepc/enums.h; touch $@
libepc/enums.h: libepc/enums.h.stamp

libepc/enums.c.tmp: libepc/enums.c.in $(libepc_headers)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template $< $(libepc_headers)) > $@ || { rm -f $@; false; }
libepc/enums.c.stamp: libepc/enums.c.tmp
	cmp -s $< libepc/enums.c || cp $< libepc/enums.c; touch $@
libepc/enums.c: libepc/enums.c.stamp

# =========================
# -- GTK-DOC INTEGRATION --
# =========================

all-local: $(lib_LTLIBRARIES)
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) all
clean-local:
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) clean
	rm -f $(BUILT_SOURCES) examples/.libs/*
	rm -f libepc/*.stamp libepc/*.tmp
	rm -f tests/*.err tests/*.out

distclean-local:
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) distclean
docs: $(lib_LTLIBRARIES)
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) docs
install-data-local:
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) install-data
maintainer-clean-local:
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) maintainer-clean
uninstall-local:
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) uninstall

dist-hook:
	mkdir -p $(gtkdoc_distdir)
	cp $(gtkdoc_srcdir)/Makefile.am $(gtkdoc_srcdir)/Makefile.in $(gtkdoc_distdir)
	cd $(gtkdoc_builddir) && $(MAKE) $(AM_MAKEFLAGS) \
	  distdir="../../../$(gtkdoc_distdir)" \
          top_distdir="../../../$(top_distdir)" \
          dist-hook

.PHONY: docs

# =====================
# -- LOCAL OVERRIDES --
# =====================

-include Makefile.local
