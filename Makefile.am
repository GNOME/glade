## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps widgets doc

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES = glade-3.desktop.in
DESKTOP_FILES =$(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

appicondir = $(datadir)/icons/hicolor/48x48/apps
appicon_DATA = glade-3.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
                $(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
                echo "***   $(gtk_update_icon_cache)"; \
	fi


EXTRA_DIST = \
	COPYING.GPL \
	COPYING.LGPL \
	glade-3.desktop.in \
	glade-3.png \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in


CLEANFILES = $(DESKTOP_FILES)

DISTCLEANFILES = intltool-extract intltool-merge intltool-update
