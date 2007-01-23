## Process this file with automake to produce Makefile.in

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
	

pixmapsdir = $(pkgdatadir)/pixmaps
pixmaps_DATA = selector.png devhelp.png plus.png

	
EXTRA_DIST = glade-3.png $(pixmaps_DATA)
