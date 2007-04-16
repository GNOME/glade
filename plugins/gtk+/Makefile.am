## Process this file with automake to produce Makefile.in

SUBDIRS = icons

libgladeui = $(top_builddir)/gladeui/libgladeui-1.la


# libgladegtk

gladegtk_LTLIBRARIES = libgladegtk.la
gladegtkdir = $(pkglibdir)/modules

libgladegtk_la_CPPLAGS =   \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	$(AM_CPPFLAGS)

libgladegtk_la_CFLAGS =     \
	-g @GCC_WARN_FLAGS@ \
	-I$(top_srcdir)     \
	-I$(top_srcdir)     \
	-I$(top_builddir)   \
	$(GTK_CFLAGS)	    \
	$(AM_CFLAGS)

libgladegtk_la_SOURCES     = glade-gtk.c glade-gtk.h
libgladegtk_la_LDFLAGS     = -module -avoid-version $(AM_LDFLAGS)
libgladegtk_la_LIBADD      = $(libgladeui) $(GTK_LIBS)

if PLATFORM_WIN32
  libgladegtk_la_LDFLAGS += -no-undefined
endif


# catalog data

catalogsdir = $(pkgdatadir)/catalogs

catalogs_DATA = gtk+.xml gtk+.xml.in

if HAVE_GTK_UNIX_PRINT
catalogs_DATA += gtkunixprint.xml gtkunixprint.xml.in
endif

@INTLTOOL_XML_NOMERGE_RULE@


CLEANFILES = gtk+.xml gtkunixprint.xml

EXTRA_DIST = gtk+.xml gtk+.xml.in gtkunixprint.xml gtkunixprint.xml.in
