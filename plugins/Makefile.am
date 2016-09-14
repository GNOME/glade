## Process this file with automake to produce Makefile.in

SUBDIRS = gtk+

if BUILD_PYTHON
SUBDIRS += python
endif

if BUILD_GLADEUI
SUBDIRS += gladeui
endif

if BUILD_WEBKIT2GTK
SUBDIRS += webkit2gtk
endif

dtddir = $(pkgdatadir)/catalogs
dtd_DATA = glade-catalog.dtd

EXTRA_DIST = glade-catalog.dtd gtk-private/glade-gtk-private.xml
