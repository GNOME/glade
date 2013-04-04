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

# Namespace expected names and naming exclusivity tests
TEST_PROGS              = create-widgets
create_widgets_CPPFLAGS = $(progs_cppflags)
create_widgets_CFLAGS   = $(progs_cflags)
create_widgets_LDFLAGS  = $(progs_libs)
create_widgets_LDADD    = $(progs_ldadd)
create_widgets_SOURCES  = create-widgets.c

TESTS = $(TEST_PROGS)
TESTS_ENVIRONMENT=$(GLADE_TEST_ENVIRONMENT)
