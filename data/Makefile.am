## Process this file with automake to produce Makefile.in

SUBDIRS = icons gettext

desktopdir = $(datadir)/applications
desktop_in_files = org.gnome.Glade.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gladeui-2.0.pc

appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = org.gnome.Glade.appdata.xml.in
$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = 				\
	$(appdata_in_files)		\
	$(desktop_in_files)		\
	gladeui-2.0.pc.in

CLEANFILES =	 			\
	$(desktop_DATA)			\
	$(pkgconfig_DATA)		\
	$(appdata_DATA)
