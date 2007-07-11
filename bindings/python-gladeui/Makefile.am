INCLUDES = $(PYTHON_INCLUDES) \
	   $(PYGTK_CFLAGS)    \
	   $(GTK_CFLAGS)      \
           -I$(top_builddir)/gladeui

pygladeuidir = $(pyexecdir)/gladeui

pygladeui_PYTHON = __init__.py

pygladeui_LTLIBRARIES = _gladeui.la

_gladeui_la_CFLAGS = $(top_srcdir)
_gladeui_la_LDFLAGS = -module -avoid-version -export-symbols-regex init_gladeui
_gladeui_la_LIBADD = $(top_builddir)/gladeui/libgladeui-1.la
_gladeui_la_SOURCES = gladeuimodule.c
nodist__gladeui_la_SOURCES = gladeui.c

EXTRA_DIST = gladeui.defs gladeui.override
CLEANFILES = gladeui.c

gladeui.c: gladeui.defs gladeui.override
	$(PYGTK_CODEGEN) \
	--prefix gladeui \
	--override gladeui.override \
	--register $(PYGTK_DEFSDIR)/gtk-types.defs \
	--register $(PYGTK_DEFSDIR)/gdk-types.defs \
	gladeui.defs > $@

