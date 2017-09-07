## Process this file with automake to produce Makefile.in

libgladeui = $(top_builddir)/gladeui/libgladeui-2.la


# libgladewebkit2gtk

gladewebkit2gtk_LTLIBRARIES = libgladewebkit2gtk.la
gladewebkit2gtkdir = $(pkglibdir)/modules

libgladewebkit2gtk_la_CPPFLAGS =              \
	-DG_LOG_DOMAIN=\"GladeUI-WEBKIT2GTK\" \
	-I$(top_srcdir)                       \
	-I$(top_builddir)                     \
	$(WEBKIT2GTK_INCLUDES)

libgladewebkit2gtk_la_CFLAGS =  \
	$(AM_CFLAGS)            \
	$(PLUGINS_WARN_CFLAGS)  \
	$(WEBKIT2GTK_CFLAGS)

libgladewebkit2gtk_la_SOURCES = glade-webkit2gtk.c

libgladewebkit2gtk_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS) $(WEBKIT2GTK_LDFLAGS)
libgladewebkit2gtk_la_LIBADD = $(libgladeui) $(WEBKIT2GTK_LIBS)

if PLATFORM_WIN32
libgladewebkit2gtk_la_LDFLAGS += -no-undefined
endif

# catalog data
catalogsdir = $(pkgdatadir)/catalogs

catalogs_in_files = webkit2gtk.xml.in

catalogs_DATA = $(catalogs_in_files:.xml.in=.xml)

EXTRA_DIST =  $(catalogs_in_files) $(catalog_DATA)

CLEANFILES = $(catalog_DATA)
