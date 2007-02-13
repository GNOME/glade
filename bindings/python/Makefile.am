## Process this file with automake to produce Makefile.in

libgladeui = $(top_builddir)/gladeui/libgladeui-1.la


# libgladepython

gladepython_LTLIBRARIES = libgladepython.la
gladepythondir = $(pkglibdir)/bindings

libgladepython_la_SOURCES = glade-python.c glade-python-gwa.c
libgladepython_la_CPPFLAGS = \
	-I$(top_srcdir)      \
	-I$(top_builddir)    \
	$(AM_CPPFLAGS)

libgladepython_la_CFLAGS = \
	-g -Wall           \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	$(PYTHON_CFLAGS)   \
	$(NO_STRICT_ALIASING_CFLAGS) \
	$(PYGTK_CFLAGS)	   \
	$(GTK_CFLAGS)      \
	$(AM_CFLAGS)

libgladepython_la_LDFLAGS = -module -avoid-version $(PYTHON_LIBS) $(AM_LDFLAGS)
libgladepython_la_LIBADD = $(libgladeui) $(PYTHON_LIBS)

if WITH_WIN32
  libgladepython_la_LDFLAGS += -no-undefined
endif
