## Process this file with automake to produce Makefile.in

SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = glade.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gladeui-2.0.pc

EXTRA_DIST = 				\
	$(desktop_in_files)		\
	gladeui-2.0.pc.in

CLEANFILES =	 			\
	$(desktop_DATA)			\
	$(pkgconfig_DATA)
