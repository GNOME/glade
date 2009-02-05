## Process this file with automake to produce Makefile.in

SUBDIRS = gtk+

if BUILD_GNOME
SUBDIRS += gnome
endif

if BUILD_PYTHON
SUBDIRS += python
endif

dtddir = $(pkgdatadir)/catalogs
dtd_DATA = glade-catalog.dtd

EXTRA_DIST = glade-catalog.dtd