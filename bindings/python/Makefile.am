## Process this file with automake to produce Makefile.in

INCLUDES = $(PYTHON_INCLUDES) \
	   $(PYTHON_CFLAGS)   \
	   $(PYGTK_CFLAGS)    \
	   -I$(top_srcdir)    \
	   -I$(top_builddir)  \
           -I$(top_builddir)/gladeui    \
	   $(NO_STRICT_ALIASING_CFLAGS) \
	   $(GTK_CFLAGS)                \
	   $(WARN_CFLAGS)               \
	   $(AM_CFLAGS)

libgladeui = $(top_builddir)/gladeui/libgladeui-1.la

EXTRA_DIST = gladeui.defs gladeui.override
CLEANFILES = gladeui.c

# libgladepython

gladepython_LTLIBRARIES = libgladepython.la
gladepythondir = $(pkglibdir)/bindings

libgladepython_la_SOURCES = glade-python.c
libgladepython_la_CPPFLAGS = \
	-I$(top_srcdir)      \
	-I$(top_builddir)    \
	$(AM_CPPFLAGS)

libgladepython_la_LDFLAGS = -module -avoid-version -export-symbols-regex glade_binding_init
libgladepython_la_LIBADD = \
	$(libgladeui)          \
	$(PYTHON_LIB_LOC)      \
	$(PYTHON_LIBS)         \
	$(PYTHON_EXTRA_LIBS)   \
	$(PYGTK_LIBS)

if PLATFORM_WIN32
libgladepython_la_LDFLAGS += -no-undefined
endif


# libgladeui python bindings


pygladeuidir = $(pyexecdir)/gladeui

pygladeui_PYTHON = __init__.py

pygladeui_LTLIBRARIES = _gladeui.la


_gladeui_la_LDFLAGS = -module -avoid-version -export-symbols-regex init_gladeui
_gladeui_la_LIBADD = \
	$(libgladeui)          \
	$(PYTHON_LIB_LOC)      \
	$(PYTHON_LIBS)         \
	$(PYTHON_EXTRA_LIBS)   \
	$(PYGTK_LIBS)
	
_gladeui_la_SOURCES = gladeuimodule.c
nodist__gladeui_la_SOURCES = gladeui.c

gladeui.c: gladeui.defs gladeui.override
	$(PYGTK_CODEGEN) \
	--prefix gladeui \
	--override gladeui.override \
	--register $(PYGTK_DEFSDIR)/gtk-types.defs \
	--register $(PYGTK_DEFSDIR)/gdk-types.defs \
	gladeui.defs > $@


