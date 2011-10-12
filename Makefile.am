# ===========================
# -- CONFIGURATION OPTIONS --
# ===========================

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
TESTS_ENVIRONMENT = $(top_srcdir)/tests/test-runner.sh
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

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
	tests/test-dispatcher-unique \
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
	$(srcdir)/libepc/consumer.h \
	$(srcdir)/libepc/contents.h \
	$(srcdir)/libepc/dispatcher.h \
	$(srcdir)/libepc/protocol.h \
	$(srcdir)/libepc/publisher.h \
	$(srcdir)/libepc/service-info.h \
	$(srcdir)/libepc/service-monitor.h \
	$(srcdir)/libepc/service-type.h \
	$(srcdir)/libepc/shell.h \
	$(srcdir)/libepc/tls.h
libepc_include_HEADERS = \
	$(libepc_headers) \
	libepc/enums.h \
	libepc/marshal.h
libepc_ui_include_HEADERS = \
	$(srcdir)/libepc-ui/password-dialog.h \
	$(srcdir)/libepc-ui/progress-window.h
pkgconfig_DATA = \
	libepc-1.0.pc \
	libepc-ui-1.0.pc
dist_noinst_SCRIPTS = \
	$(srcdir)/tests/test-runner.sh
dist_noinst_DATA = \
	$(srcdir)/examples/publisher.ui
	$(srcdir)/gtk-doc.make \
	$(gtkdoc_srcdir)/libepc-1.0-docs.xml \
	$(gtkdoc_srcdir)/libepc-1.0-sections.txt \
	$(srcdir)/intltool-extract.in \
	$(srcdir)/intltool-merge.in \
	$(srcdir)/intltool-update.in
DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

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
	$(WARN_CFLAGS) \
	$(DISABLE_DEPRECATED) \
	-DLOCALEDIR=\"$(localedir)\" \
	-DGETTEXT_PACKAGE=\"@GETTEXT_PACKAGE@\"

example_epc_cflags = $(AM_CFLAGS) $(LIBEPC_CFLAGS)
example_epc_libs = libepc/libepc-1.0.la $(LIBEPC_LIBS)
example_epc_ui_cflags = $(example_epc_cflags) $(LIBEPC_UI_CFLAGS)
example_epc_ui_libs = $(example_epc_libs) libepc-ui/libepc-ui-1.0.la $(LIBEPC_UI_LIBS)

libepc_libepc_1_0_la_CFLAGS			= $(AM_CFLAGS) $(LIBEPC_CFLAGS) -DG_LOG_DOMAIN=\"libepc\"
libepc_libepc_1_0_la_LDFLAGS			= $(LIBEPC_LIBS) -version-info $(LT_VERSION_INFO)

libepc_ui_libepc_ui_1_0_la_CFLAGS		= $(AM_CFLAGS) $(LIBEPC_UI_CFLAGS) -DG_LOG_DOMAIN=\"libepc-ui\"
libepc_ui_libepc_ui_1_0_la_LDFLAGS		= $(LIBEPC_UI_LIBS) -version-info $(LT_VERSION_INFO)
libepc_ui_libepc_ui_1_0_la_LIBADD		= libepc/libepc-1.0.la

examples_list_resources_CFLAGS			= $(example_epc_cflags)
examples_list_resources_LDADD			= $(example_epc_libs)
examples_lookup_resource_CFLAGS			= $(example_epc_cflags)
examples_lookup_resource_LDADD			= $(example_epc_libs)
examples_service_browser_CFLAGS			= $(example_epc_cflags)
examples_service_browser_LDADD			= $(example_epc_libs)
examples_simple_publisher_CFLAGS		= $(example_epc_cflags)
examples_simple_publisher_LDADD			= $(example_epc_libs)

examples_consumer_ui_CFLAGS			= $(example_epc_ui_cflags) $(AVAHI_UI_CFLAGS)
examples_consumer_ui_LDADD			= $(example_epc_ui_libs) $(AVAHI_UI_LIBS)
examples_publisher_ui_CFLAGS			= $(example_epc_ui_cflags) $(AVAHI_UI_CFLAGS) -rdynamic
examples_publisher_ui_LDADD			= $(example_epc_ui_libs) $(AVAHI_UI_LIBS)
examples_server_credentials_CFLAGS		= $(example_epc_ui_cflags)
examples_server_credentials_LDADD		= $(example_epc_ui_libs)


test_epc_libs = tests/libepc-tests.la $(example_epc_libs)
test_epc_ui_libs = tests/libepc-tests.la $(example_epc_ui_libs)

tests_libepc_tests_la_CFLAGS			= $(example_epc_cflags)

tests_test_consumer_by_info_CFLAGS		= $(example_epc_cflags)
tests_test_consumer_by_info_LDADD		= $(test_epc_libs)
tests_test_consumer_by_name_CFLAGS		= $(example_epc_cflags)
tests_test_consumer_by_name_LDADD		= $(test_epc_libs)
tests_test_dispatcher_local_collision_CFLAGS	= $(example_epc_cflags)
tests_test_dispatcher_local_collision_LDADD	= $(test_epc_libs)
tests_test_dispatcher_multiple_services_CFLAGS	= $(example_epc_cflags)
tests_test_dispatcher_multiple_services_LDADD	= $(test_epc_libs)
tests_test_dispatcher_rename_CFLAGS		= $(example_epc_cflags)
tests_test_dispatcher_rename_LDADD		= $(test_epc_libs)
tests_test_dispatcher_reset_CFLAGS		= $(example_epc_cflags)
tests_test_dispatcher_reset_LDADD		= $(test_epc_libs)
tests_test_dispatcher_simple_service_CFLAGS	= $(example_epc_cflags)
tests_test_dispatcher_simple_service_LDADD	= $(test_epc_libs)
tests_test_dispatcher_subtypes_CFLAGS		= $(example_epc_cflags)
tests_test_dispatcher_subtypes_LDADD		= $(test_epc_libs)
tests_test_dispatcher_unique_CFLAGS		= $(example_epc_cflags)
tests_test_dispatcher_unique_LDADD		= $(test_epc_libs)
tests_test_expand_name_CFLAGS			= $(example_epc_cflags)
tests_test_expand_name_LDADD			= $(test_epc_libs)
tests_test_progress_hooks_CFLAGS		= $(example_epc_ui_cflags)
tests_test_progress_hooks_LDADD			= $(test_epc_ui_libs)
tests_test_publisher_bookmarks_CFLAGS		= $(example_epc_cflags)
tests_test_publisher_bookmarks_LDADD		= $(test_epc_libs)
tests_test_publisher_change_name_CFLAGS		= $(example_epc_cflags)
tests_test_publisher_change_name_LDADD		= $(test_epc_libs)
tests_test_publisher_libsoup_494128_CFLAGS	= $(example_epc_cflags)
tests_test_publisher_libsoup_494128_LDADD	= $(test_epc_libs)
tests_test_publisher_unique_CFLAGS		= $(example_epc_cflags)
tests_test_publisher_unique_LDADD		= $(test_epc_libs)
tests_test_service_type_CFLAGS			= $(example_epc_cflags)
tests_test_service_type_LDADD			= $(test_epc_libs)

# ==================
# -- CUSTOM RULES --
# ==================

libepc/marshal.h: $(srcdir)/libepc/marshal.list
	{ echo '/* Generated by $(GLIB_GENMARSHAL) from $< */' && \
	  $(GLIB_GENMARSHAL) $< --header --prefix=_epc_marshal; } > $@ || \
	{ rm -f $@; false; }
libepc/marshal.c: $(srcdir)/libepc/marshal.list
	{ echo '/* Generated by $(GLIB_GENMARSHAL) from $< */' && \
	  echo '#include "marshal.h"' && \
	  $(GLIB_GENMARSHAL) $< --body --prefix=_epc_marshal; } > $@ || \
	{ rm -f $@; false; }

libepc/enums.h.tmp: $(srcdir)/libepc/enums.h.in $(libepc_headers)
	$(GLIB_MKENUMS) --template $< $(libepc_headers) > $@ || { rm -f $@; false; }
libepc/enums.h.stamp: libepc/enums.h.tmp
	cmp -s $< libepc/enums.h || cp $< libepc/enums.h; touch $@
libepc/enums.h: libepc/enums.h.stamp

libepc/enums.c.tmp: $(srcdir)/libepc/enums.c.in $(libepc_headers)
	$(GLIB_MKENUMS) --template $< $(libepc_headers) > $@ || { rm -f $@; false; }
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
SUBDIRS = po

# =====================
# -- LOCAL OVERRIDES --
# =====================

-include Makefile.local
