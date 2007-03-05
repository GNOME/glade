## Process this file with automake to produce Makefile.in

SUBDIRS = icons

libgladeui = $(top_builddir)/gladeui/libgladeui-1.la


# libgladegnome

gladegnome_LTLIBRARIES = libgladegnome.la
gladegnomedir = $(pkglibdir)/modules

libgladegnome_la_SOURCES = glade-gnome.c
libgladegnome_la_CPPFLAGS = $(AM_CPPFLAGS)
libgladegnome_la_CFLAGS = \
	-g @GCC_WARN_FLAGS@          \
	-I$(top_srcdir)              \
	-I$(top_builddir)            \
	-I$(top_srcdir)/plugins/gtk+ \
	$(GNOME_CFLAGS)              \
	$(AM_CPPFLAGS)
libgladegnome_la_LDFLAGS = -module -avoid-version $(GNOME_LIBS) $(AM_LDFLAGS)
libgladegnome_la_LIBADD = $(libgladeui) $(GLADE_LIBS)

if WITH_WIN32
  libgladegnome_la_LDFLAGS += -no-undefined
endif


# catalog data

catalogsdir = $(pkgdatadir)/catalogs
catalogs_DATA = bonobo.xml \
                canvas.xml \
                gnome.xml \
                bonobo.xml.in \
                canvas.xml.in \
                gnome.xml.in
@INTLTOOL_XML_NOMERGE_RULE@


CLEANFILES = bonobo.xml canvas.xml gnome.xml

EXTRA_DIST = $(catalogs_DATA)
