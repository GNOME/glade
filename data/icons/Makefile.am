## Process this file with automake to produce Makefile.in

scalableappicondir = $(datadir)/icons/hicolor/scalable/apps
scalableappicon_DATA = \
	hicolor/scalable/apps/org.gnome.Glade.svg \
	$(NULL)

symbolicappicondir = $(datadir)/icons/hicolor/symbolic/apps
symbolicappicon_DATA = \
	hicolor/symbolic/apps/org.gnome.Glade-symbolic.svg \
	hicolor/symbolic/apps/glade-brand-symbolic.svg \
	$(NULL)

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
pixmaps_DATA = devhelp.png \
	plus.png \
	fixed-bg.png placeholder.png \
	deprecated-16x16.png \
	deprecated-22x22.png


EXTRA_DIST = \
	$(scalableappicon_DATA) \
	$(symbolicappicon_DATA) \
	$(pixmaps_DATA) \
	glade.ico
