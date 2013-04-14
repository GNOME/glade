
libgladeui = $(top_builddir)/gladeui/libgladeui-2.la

# libgladegtk

gladeglade_LTLIBRARIES = libgladeglade.la
gladegladedir = $(pkglibdir)/modules

libgladeglade_la_CPPFLAGS =   \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DG_LOG_DOMAIN=\"GladeUI-Glade\" \
	$(GTK_CFLAGS)	               \
	$(PLUGINS_WARN_CFLAGS)         \
	$(AM_CPPFLAGS)

libgladeglade_la_CFLAGS      = $(AM_CFLAGS)
libgladeglade_la_LDFLAGS     = -module -avoid-version $(AM_LDFLAGS)
libgladeglade_la_LIBADD      = $(libgladeui) $(GTK_LIBS)
libgladeglade_la_SOURCES     = 		\
	glade-glade-editor-skeleton.c	\
	glade-glade-property-shell.c

catalogsdir = $(pkgdatadir)/catalogs

catalogs_DATA = gladeui.xml

EXTRA_DIST = $(catalogs_DATA)
