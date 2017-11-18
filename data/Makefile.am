## Process this file with automake to produce Makefile.in

SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = glade.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gladeui-2.0.pc

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = glade.appdata.xml.in

EXTRA_DIST = 				\
	$(appdata_in_files)		\
	$(desktop_in_files)		\
	gladeui-2.0.pc.in

CLEANFILES =	 			\
	$(desktop_DATA)			\
	$(pkgconfig_DATA)		\
	$(appdata_DATA)
