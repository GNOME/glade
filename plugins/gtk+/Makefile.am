## Process this file with automake to produce Makefile.in

SUBDIRS = icons

libgladeui = $(top_builddir)/gladeui/libgladeui-2.la


# libgladegtk

gladegtk_LTLIBRARIES = libgladegtk.la
gladegtkdir = $(pkglibdir)/modules

libgladegtk_la_CPPFLAGS =   \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DG_LOG_DOMAIN=\"GladeUI-GTK\" \
	$(GTK_CFLAGS)	               \
	$(PLUGINS_WARN_CFLAGS)         \
	$(AM_CPPFLAGS)

libgladegtk_la_CFLAGS = $(AM_CFLAGS)

libgladegtk_la_SOURCES = 		\
	$(BUILT_SOURCES) 		\
	glade-accels.c			\
	glade-activatable-editor.c	\
	glade-attributes.c		\
	glade-button-editor.c		\
	glade-cell-renderer-editor.c	\
	glade-column-types.c		\
	glade-entry-editor.c		\
	glade-fixed.c			\
	glade-gtk.c			\
	glade-gtk-action-widgets.c	\
	glade-gtk-box.c			\
	glade-gtk-grid.c		\
	glade-gtk-info-bar.c		\
	glade-gtk-switch.c		\
	glade-gtk-table.c		\
	glade-icon-factory-editor.c	\
	glade-icon-sources.c		\
	glade-image-editor.c		\
	glade-image-item-editor.c	\
	glade-label-editor.c		\
	glade-model-data.c		\
	glade-store-editor.c		\
	glade-string-list.c		\
	glade-tool-button-editor.c	\
	glade-tool-item-group-editor.c	\
	glade-treeview-editor.c		\
	glade-widget-editor.c

libgladegtk_la_LDFLAGS     = -module -avoid-version $(AM_LDFLAGS)
libgladegtk_la_LIBADD      = $(libgladeui) $(GTK_LIBS)

noinst_HEADERS =			\
	glade-accels.h			\
	glade-activatable-editor.h	\
	glade-attributes.h		\
	glade-button-editor.h		\
	glade-cell-renderer-editor.h	\
	glade-column-types.h		\
	glade-entry-editor.h		\
	glade-fixed.h			\
	glade-gtk.h			\
	glade-gtk-action-widgets.h	\
	glade-icon-factory-editor.h	\
	glade-icon-sources.h		\
	glade-image-editor.h		\
	glade-image-item-editor.h	\
	glade-label-editor.h		\
	glade-model-data.h		\
	glade-store-editor.h		\
	glade-string-list.h		\
	glade-tool-button-editor.h	\
	glade-tool-item-group-editor.h	\
	glade-treeview-editor.h		\
	glade-widget-editor.h

if PLATFORM_WIN32
libgladegtk_la_LDFLAGS += -no-undefined
endif


# catalog data

catalogsdir = $(pkgdatadir)/catalogs

catalogs_DATA = gtk+.xml

if HAVE_GTK_UNIX_PRINT
catalogs_DATA += gtkunixprint.xml
endif

@INTLTOOL_XML_NOMERGE_RULE@

BUILT_SOURCES = 		\
	glade-gtk-marshallers.c \
	glade-gtk-marshallers.h \
	glade-gtk-resources.c 	\
	glade-gtk-resources.h

UI_FILES = 				\
	glade-activatable-editor.ui	\
	glade-button-editor.ui		\
	glade-entry-editor.ui		\
	glade-image-editor.ui		\
	glade-label-editor.ui		\
	glade-tool-button-editor.ui	\
	glade-widget-editor.ui

EXTRA_DIST = 					\
	$(UI_FILES)				\
	glade-gtk-marshallers.list 		\
	gtk+.xml gtk+.xml.in 			\
	gtkunixprint.xml gtkunixprint.xml.in 	\
	glade-gtk-resources.gresource.xml

CLEANFILES = gtk+.xml gtkunixprint.xml $(BUILT_SOURCES)

#
# Marshaller generation
#
glade-gtk-marshallers.h: glade-gtk-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=glade_gtk_marshal $(srcdir)/glade-gtk-marshallers.list --header > glade-gtk-marshallers-h.tmp \
	&& mv glade-gtk-marshallers-h.tmp glade-gtk-marshallers.h \
	|| ( rm -f glade-gtk-marshallers-h.tmp && exit 1)
glade-gtk-marshallers.c: glade-gtk-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=glade_gtk_marshal $(srcdir)/glade-gtk-marshallers.list --body --header > glade-gtk-marshallers-c.tmp     \
	&& mv glade-gtk-marshallers-c.tmp glade-gtk-marshallers.c \
	|| ( rm -f glade-gtk-marshallers-c.tmp && exit 1 )

# Generate resources
glade-gtk-resources.h: glade-gtk-resources.gresource.xml $(UI_FILES) $(GRAPHICS_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/glade-gtk-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _glade_gtk --generate-header
glade-gtk-resources.c: glade-gtk-resources.gresource.xml $(UI_FILES) $(GRAPHICS_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/glade-gtk-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _glade_gtk --generate-source
