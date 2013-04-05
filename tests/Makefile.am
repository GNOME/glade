include $(top_srcdir)/glade-rules.mk

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

# Test that GladeWidgets and thier runtime objects
# are created and finalized properly
TEST_PROGS              = create-widgets
create_widgets_CPPFLAGS = $(progs_cppflags)
create_widgets_CFLAGS   = $(progs_cflags)
create_widgets_LDFLAGS  = $(progs_libs)
create_widgets_LDADD    = $(progs_ldadd)
create_widgets_SOURCES  = create-widgets.c

# Test that various types of container widgets
# successfully parent child types via the
# GladeWidget / adaptor code
TEST_PROGS        += add-child
add_child_CPPFLAGS = $(progs_cppflags)
add_child_CFLAGS   = $(progs_cflags)
add_child_LDFLAGS  = $(progs_libs)
add_child_LDADD    = $(progs_ldadd)
add_child_SOURCES  = add-child.c

TESTS = $(TEST_PROGS)
TESTS_ENVIRONMENT=$(GLADE_TEST_ENVIRONMENT)
