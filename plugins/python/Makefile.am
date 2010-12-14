## Process this file with automake to produce Makefile.in

libgladeui = $(top_builddir)/gladeui/libgladeui-1.la


# libgladepython

gladepython_LTLIBRARIES = libgladepython.la
gladepythondir = $(pkglibdir)/modules

libgladepython_la_SOURCES = glade-python.c
libgladepython_la_CPPFLAGS = $(AM_CPPFLAGS)
libgladepython_la_CFLAGS = \
        -DG_LOG_DOMAIN=\"GladeUI-PYTHON\" \
	-I$(top_srcdir)                  \
	-I$(top_builddir)                \
	$(PYTHON_INCLUDES)      \
	$(PYGTK_CFLAGS)            \
	$(GTK_CFLAGS)                    \
	$(PLUGINS_WARN_CFLAGS)           \
	$(AM_CPPFLAGS)
libgladepython_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS)
libgladepython_la_LIBADD = $(libgladeui) $(PYTHON_LIBS)

if PLATFORM_WIN32
libgladepython_la_LDFLAGS += -no-undefined
endif

