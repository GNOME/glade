@INTLTOOL_XML_NOMERGE_RULE@

CLEANFILES = gtk+.xml

catalogsdir = $(glade_catalogsdir)

catalogs_DATA = gtk+.xml

if BUILD_GNOME
GNOME_CATALOGS = bonobo.xml canvas.xml gnome.xml
CLEANFILES +=  $(GNOME_CATALOGS)
catalogs_DATA += $(GNOME_CATALOGS)
endif

EXTRA_DIST = \
	$(catalogs_DATA)

