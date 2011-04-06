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

libgladegtk_la_SOURCES = \
	glade-gtk.c \
	glade-fixed.c \
	glade-accels.c \
	glade-attributes.c \
	glade-column-types.c \
	glade-model-data.c \
	glade-text-button.c \
	glade-icon-sources.c \
	glade-button-editor.c \
	glade-tool-button-editor.c \
	glade-image-editor.c \
	glade-image-item-editor.c \
	glade-icon-factory-editor.c \
	glade-store-editor.c \
	glade-label-editor.c \
	glade-cell-renderer-editor.c \
	glade-treeview-editor.c \
	glade-entry-editor.c \
	glade-activatable-editor.c \
	glade-tool-item-group-editor.c \
	glade-string-list.c \
	glade-gtk-table.c \
	glade-gtk-grid.c \
	glade-gtk-action-widgets.c \
	glade-gtk-info-bar.c \
	glade-gtk-activatable.c \
	glade-gtk-switch.c \
	glade-gtk-marshallers.c

libgladegtk_la_LDFLAGS     = -module -avoid-version $(AM_LDFLAGS)
libgladegtk_la_LIBADD      = $(libgladeui) $(GTK_LIBS)

noinst_HEADERS = \
	glade-gtk.h \
	glade-accels.h \
	glade-attributes.h \
	glade-column-types.h \
	glade-model-data.h \
	glade-text-button.h \
	glade-icon-sources.h \
	glade-button-editor.h \
	glade-tool-button-editor.h \
	glade-image-editor.h \
	glade-image-item-editor.h \
	glade-icon-factory-editor.h \
	glade-store-editor.h \
	glade-label-editor.h \
	glade-cell-renderer-editor.h \
	glade-treeview-editor.h \
	glade-entry-editor.h \
	glade-activatable-editor.h \
	glade-fixed.h \
	glade-tool-item-group-editor.h \
	glade-string-list.h \
	glade-gtk-marshallers.h \
	glade-gtk-activatable.h \
	glade-gtk-action-widgets.h


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


CLEANFILES = gtk+.xml gtkunixprint.xml

BUILT_SOURCES = glade-gtk-marshallers.c glade-gtk-marshallers.h

EXTRA_DIST = glade-gtk-marshallers.list gtk+.xml gtk+.xml.in gtkunixprint.xml gtkunixprint.xml.in

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
