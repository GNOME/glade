## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps widgets

Developmentdir = $(datadir)/applications
Development_in_files = glade-3.desktop.in
Development_DATA = $(Development_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	glade-3.desktop.in \
	glade-3.png \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = glade-3.png
