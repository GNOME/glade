## Process this file with automake to produce Makefile.in

SUBDIRS = src po pixmaps widgets

Developmentdir = $(datadir)/applications
Development_in_files = glade-2.desktop.in
Development_DATA = $(Development_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	glade-2.desktop.in \
	glade-2.png \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = glade-2.png

# Now, we are using pkg-config so, we don't need the m4 macros anymore ;-)
# I suppose that we could disable this.

# Install all the Gnome m4 macros we use to build Glade, so we can copy them
# to each project's directory (for Gnome projects).
#install-data-local:
#	@$(NORMAL_INSTALL)
#	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/gnome
#	$(INSTALL_DATA) $(srcdir)/macros/Makefile.am $(DESTDIR)$(pkgdatadir)/gnome
#	$(INSTALL_DATA) $(srcdir)/macros/autogen.sh $(DESTDIR)$(pkgdatadir)/gnome
#	for macro_file in $(srcdir)/macros/*.m4; do \
#	  $(INSTALL_DATA) $$macro_file $(DESTDIR)$(pkgdatadir)/gnome; \
#	done

