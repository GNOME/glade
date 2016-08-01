## Process this file with automake to produce Makefile.in

libgladeui = $(top_builddir)/gladeui/libgladeui-2.la


# libgladegjs

gladegjs_LTLIBRARIES = libgladegjs.la
gladegjsdir = $(pkglibdir)/modules

libgladegjs_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"GladeUI-GJS\" \
	-I$(top_srcdir)                \
	-I$(top_builddir)              \
	$(GJS_INCLUDES)

libgladegjs_la_CFLAGS = \
	$(AM_CFLAGS)                   \
	$(PLUGINS_WARN_CFLAGS)         \
	$(GJS_CFLAGS)

libgladegjs_la_SOURCES = glade-gjs.c

libgladegjs_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS) $(GJS_LDFLAGS)
libgladegjs_la_LIBADD = $(libgladeui) $(GJS_LIBS)

if PLATFORM_WIN32
libgladegjs_la_LDFLAGS += -no-undefined
endif

