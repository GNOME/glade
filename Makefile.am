## Process this file with automake to produce Makefile.in

SUBDIRS = macros intl src po images widgets

Developmentdir = $(datadir)/gnome/apps/Development
Development_in_files = glade2.desktop.in
Development_DATA = $(Development_in_files:.desktop.in=.desktop)

@XML_I18N_MERGE_DESKTOP_RULE@

EXTRA_DIST = \
	glade2.spec.in \
	glade2.desktop.in \
	glade2.desktop \
	glade2.png \
	xml-i18n-extract.in \
	xml-i18n-merge.in \
	xml-i18n-update.in

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = glade2.png

dist-hook: glade2.spec
	cp glade2.spec $(distdir)


# Install all the Gnome m4 macros we use to build Glade, so we can copy them
# to each project's directory (for Gnome projects).
install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/gnome
	$(INSTALL_DATA) $(srcdir)/macros/Makefile.am $(DESTDIR)$(pkgdatadir)/gnome
	$(INSTALL_DATA) $(srcdir)/macros/autogen.sh $(DESTDIR)$(pkgdatadir)/gnome
	for macro_file in $(srcdir)/macros/*.m4; do \
	  $(INSTALL_DATA) $$macro_file $(DESTDIR)$(pkgdatadir)/gnome; \
	done
	$(mkinstalldirs) $(DESTDIR)$(datadir)/pixmaps/glade2
	$(INSTALL_DATA) $(srcdir)/images/glade_logo.png $(DESTDIR)$(datadir)/pixmaps/glade

