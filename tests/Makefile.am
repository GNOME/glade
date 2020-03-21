include $(top_srcdir)/glade-rules.mk

TEST_PROGS = \
	create-widgets \
	add-child \
	toplevel-order

noinst_PROGRAMS = $(TEST_PROGS)

progs_cppflags = \
	$(common_defines)   \
	-I$(top_srcdir)     \
	-I$(top_builddir)   \
	$(GTK_CFLAGS)       \
	$(GTK_MAC_BUNDLE_FLAG) \
	$(GTK_MAC_CFLAGS)  \
	$(WARN_CFLAGS)      \
	$(AM_CPPFLAGS)

progs_cflags = \
	$(AM_CFLAGS)

progs_libs  = $(GTK_LIBS) $(GTK_MAC_LIBS)
progs_ldadd = $(top_builddir)/gladeui/libgladeui-2.la

# Test that GladeWidgets and their runtime objects
# are created and finalized properly
create_widgets_CPPFLAGS = $(progs_cppflags)
create_widgets_CFLAGS   = $(progs_cflags)
create_widgets_LDFLAGS  = $(progs_libs)
create_widgets_LDADD    = $(progs_ldadd)
create_widgets_SOURCES  = create-widgets.c

# Test that various types of container widgets
# successfully parent child types via the
# GladeWidget / adaptor code
add_child_CPPFLAGS = $(progs_cppflags)
add_child_CFLAGS   = $(progs_cflags)
add_child_LDFLAGS  = $(progs_libs)
add_child_LDADD    = $(progs_ldadd)
add_child_SOURCES  = add-child.c

TOPLEVEL_ORDER_FILES = \
	toplevel-order-resources.gresource.xml \
	toplevel_order_test.glade \
	toplevel_order_test.glade \
	toplevel_order_test2.glade \
	toplevel_order_test3.glade \
	toplevel_order_test4.glade \
	toplevel_order_test5.glade \
	toplevel_order_test6.glade

BUILT_SOURCES = toplevel-order-resources.c toplevel-order-resources.h
# Generate resources
toplevel-order-resources.h: $(TOPLEVEL_ORDER_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/toplevel-order-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _glade --generate-header
toplevel-order-resources.c: $(TOPLEVEL_ORDER_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/toplevel-order-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _glade --generate-source

# Test toplevel order in xml output
toplevel_order_CPPFLAGS = $(progs_cppflags)
toplevel_order_CFLAGS   = $(progs_cflags)
toplevel_order_LDFLAGS  = $(progs_libs)
toplevel_order_LDADD    = $(progs_ldadd)
toplevel_order_SOURCES  = \
	toplevel-order.c \
	toplevel-order-resources.c

noinst_HEADERS = \
	toplevel-order-resources.h

EXTRA_DIST = $(TOPLEVEL_ORDER_FILES)

TESTS = $(TEST_PROGS)
TESTS_ENVIRONMENT=$(GLADE_TEST_ENVIRONMENT)
