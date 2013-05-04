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
	$(GLADE_DEBUG_FLAGS)		\
	$(AM_CPPFLAGS)

libgladegtk_la_CFLAGS = $(AM_CFLAGS)

libgladegtk_la_SOURCES = 		\
	$(BUILT_SOURCES) 		\
	glade-about-dialog-editor.c	\
	glade-accels.c			\
	glade-activatable-editor.c	\
	glade-attributes.c		\
	glade-button-editor.c		\
	glade-cell-renderer-editor.c	\
	glade-column-types.c		\
	glade-entry-editor.c		\
	glade-file-chooser-dialog-editor.c \
	glade-file-chooser-editor.c	\
	glade-file-chooser-widget-editor.c \
	glade-fixed.c			\
	glade-gtk-about-dialog.c	\
	glade-gtk-action.c		\
	glade-gtk-action-group.c	\
	glade-gtk-action-widgets.c	\
	glade-gtk-adjustment.c		\
	glade-gtk-assistant.c		\
	glade-gtk-box.c			\
	glade-gtk-button.c		\
	glade-gtk-cell-layout.c		\
	glade-gtk-cell-renderer.c	\
	glade-gtk-combo-box.c		\
	glade-gtk-combo-box-text.c	\
	glade-gtk-container.c		\
	glade-gtk-dialog.c		\
	glade-gtk-entry.c		\
	glade-gtk-entry-buffer.c	\
	glade-gtk-expander.c		\
	glade-gtk-file-chooser-widget.c	\
	glade-gtk-fixed-layout.c	\
	glade-gtk-frame.c		\
	glade-gtk-grid.c		\
	glade-gtk-icon-factory.c	\
	glade-gtk-image.c		\
	glade-gtk-image-menu-item.c	\
	glade-gtk-info-bar.c		\
	glade-gtk-label.c		\
	glade-gtk-list-store.c		\
	glade-gtk-menu.c		\
	glade-gtk-menu-bar.c		\
	glade-gtk-menu-item.c		\
	glade-gtk-menu-shell.c		\
	glade-gtk-menu-tool-button.c	\
	glade-gtk-message-dialog.c	\
	glade-gtk-notebook.c		\
	glade-gtk-paned.c		\
	glade-gtk-radio-button.c	\
	glade-gtk-radio-menu-item.c	\
	glade-gtk-recent-chooser-menu.c	\
	glade-gtk-recent-file-filter.c	\
	glade-gtk-size-group.c		\
	glade-gtk-spin-button.c		\
	glade-gtk-switch.c		\
	glade-gtk-table.c		\
	glade-gtk-text-buffer.c		\
	glade-gtk-text-tag-table.c	\
	glade-gtk-text-view.c		\
	glade-gtk-tool-button.c		\
	glade-gtk-tool-item.c		\
	glade-gtk-tool-item-group.c	\
	glade-gtk-tool-palette.c	\
	glade-gtk-toolbar.c		\
	glade-gtk-tree-view.c		\
	glade-gtk-widget.c		\
	glade-gtk-window.c		\
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
	glade-widget-editor.c		\
	glade-window-editor.c

libgladegtk_la_LDFLAGS     = -module -avoid-version $(AM_LDFLAGS)
libgladegtk_la_LIBADD      = $(libgladeui) $(GTK_LIBS)

noinst_HEADERS =			\
	glade-about-dialog-editor.h	\
	glade-accels.h			\
	glade-activatable-editor.h	\
	glade-attributes.h		\
	glade-button-editor.h		\
	glade-cell-renderer-editor.h	\
	glade-column-types.h		\
	glade-entry-editor.h		\
	glade-file-chooser-dialog-editor.h \
	glade-file-chooser-editor.h	\
	glade-file-chooser-widget-editor.h \
	glade-fixed.h			\
	glade-gtk.h			\
	glade-gtk-action-widgets.h	\
	glade-gtk-button.h		\
	glade-gtk-cell-layout.h		\
	glade-gtk-cell-renderer.h	\
	glade-gtk-dialog.h		\
	glade-gtk-frame.h		\
	glade-gtk-image.h		\
	glade-gtk-menu-shell.h		\
	glade-gtk-notebook.h		\
	glade-gtk-tree-view.h		\
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
	glade-widget-editor.h		\
	glade-window-editor.h

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
	glade-about-dialog-editor.ui	\
	glade-activatable-editor.ui	\
	glade-button-editor.ui		\
	glade-entry-editor.ui		\
	glade-file-chooser-dialog-editor.ui \
	glade-file-chooser-editor.ui	\
	glade-file-chooser-widget-editor.ui \
	glade-image-editor.ui		\
	glade-label-editor.ui		\
	glade-tool-button-editor.ui	\
	glade-widget-editor.ui		\
	glade-window-editor.ui

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
