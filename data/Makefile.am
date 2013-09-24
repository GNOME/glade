## Process this file with automake to produce Makefile.in

SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = glade.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gladeui-2.0.pc

appdatadir = $(datadir)/appdata
appdata_DATA = glade.appdata.xml

EXTRA_DIST = 				\
	$(desktop_in_files)		\
	gladeui-2.0.pc.in		\
	glade.appdata.xml.in

CLEANFILES =	 			\
	$(desktop_DATA)			\
	$(pkgconfig_DATA)		\
	$(appdata_DATA)
