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
	glade-action-editor.c		\
	glade-activatable-editor.c	\
	glade-app-chooser-button-editor.c \
	glade-app-chooser-widget-editor.c \
	glade-arrow-editor.c		\
	glade-attributes.c		\
	glade-action-bar-editor.c	\
	glade-box-editor.c		\
	glade-button-editor.c		\
	glade-cell-renderer-editor.c	\
	glade-column-types.c		\
	glade-combo-box-editor.c	\
	glade-combo-box-text-editor.c	\
	glade-entry-editor.c		\
	glade-file-chooser-button-editor.c \
	glade-file-chooser-dialog-editor.c \
	glade-file-chooser-editor.c	\
	glade-file-chooser-widget-editor.c \
	glade-fixed.c			\
	glade-font-button-editor.c 	\
	glade-font-chooser-dialog-editor.c \
	glade-font-chooser-editor.c	\
	glade-font-chooser-widget-editor.c \
	glade-grid-editor.c		\
	glade-gtk-about-dialog.c	\
	glade-gtk-action.c		\
	glade-gtk-action-bar.c		\
	glade-gtk-action-group.c	\
	glade-gtk-action-widgets.c	\
	glade-gtk-adjustment.c		\
	glade-gtk-app-chooser-button.c	\
	glade-gtk-app-chooser-widget.c	\
	glade-gtk-arrow.c		\
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
	glade-gtk-font-chooser-widget.c	\
	glade-gtk-frame.c		\
	glade-gtk-grid.c		\
	glade-gtk-icon-factory.c	\
	glade-gtk-icon-view.c		\
	glade-gtk-image.c		\
	glade-gtk-image-menu-item.c	\
	glade-gtk-info-bar.c		\
	glade-gtk-label.c		\
	glade-gtk-level-bar.c		\
	glade-gtk-list-box.c		\
	glade-gtk-list-store.c		\
	glade-gtk-menu.c		\
	glade-gtk-menu-bar.c		\
	glade-gtk-menu-item.c		\
	glade-gtk-menu-shell.c		\
	glade-gtk-menu-tool-button.c	\
	glade-gtk-message-dialog.c	\
	glade-gtk-model-button.c	\
	glade-gtk-notebook.c		\
	glade-gtk-overlay.c		\
	glade-gtk-paned.c		\
	glade-gtk-popover.c		\
	glade-gtk-popover-menu.c	\
	glade-gtk-progress-bar.c	\
	glade-gtk-radio-button.c	\
	glade-gtk-radio-menu-item.c	\
	glade-gtk-recent-chooser-menu.c	\
	glade-gtk-recent-chooser-widget.c \
	glade-gtk-recent-file-filter.c	\
	glade-gtk-revealer.c	\
	glade-gtk-scale.c		\
	glade-gtk-scrollbar.c		\
	glade-gtk-scrolled-window.c	\
	glade-gtk-searchbar.c		\
	glade-gtk-size-group.c		\
	glade-gtk-spin-button.c		\
	glade-gtk-stack.c		\
	glade-gtk-stack-switcher.c	\
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
	glade-gtk-viewport.c		\
	glade-gtk-widget.c		\
	glade-gtk-window.c		\
	glade-gtk-header-bar.c 		\
	glade-header-bar-editor.c	\
	glade-icon-factory-editor.c	\
	glade-icon-sources.c		\
	glade-icon-view-editor.c	\
	glade-image-editor.c		\
	glade-image-item-editor.c	\
	glade-label-editor.c		\
	glade-layout-editor.c		\
	glade-level-bar-editor.c	\
	glade-message-dialog-editor.c	\
	glade-misc-editor.c		\
	glade-model-button-editor.c	\
	glade-model-data.c		\
	glade-notebook-editor.c		\
	glade-popover-editor.c		\
	glade-popover-menu-editor.c	\
	glade-progress-bar-editor.c 	\
	glade-real-tree-view-editor.c 	\
	glade-recent-action-editor.c 	\
	glade-recent-chooser-dialog-editor.c \
	glade-recent-chooser-editor.c	\
	glade-recent-chooser-menu-editor.c \
	glade-recent-chooser-widget-editor.c \
	glade-scale-button-editor.c	\
	glade-scale-editor.c		\
	glade-scrollable-editor.c	\
	glade-scrollbar-editor.c	\
	glade-scrolled-window-editor.c	\
	glade-spin-button-editor.c	\
	glade-stack-editor.c		\
	glade-stack-switcher-editor.c	\
	glade-store-editor.c		\
	glade-string-list.c		\
	glade-text-view-editor.c	\
	glade-tool-button-editor.c	\
	glade-tool-item-group-editor.c	\
	glade-tool-palette-editor.c	\
	glade-treeview-editor.c		\
	glade-viewport-editor.c		\
	glade-widget-editor.c		\
	glade-window-editor.c

libgladegtk_la_LDFLAGS     = -module -avoid-version $(AM_LDFLAGS)
libgladegtk_la_LIBADD      = $(libgladeui) $(GTK_LIBS)

noinst_HEADERS =			\
	glade-about-dialog-editor.h	\
	glade-accels.h			\
	glade-action-editor.h		\
	glade-activatable-editor.h	\
	glade-app-chooser-button-editor.h \
	glade-app-chooser-widget-editor.h \
	glade-arrow-editor.h		\
	glade-attributes.h		\
	glade-action-bar-editor.h	\
	glade-box-editor.h		\
	glade-button-editor.h		\
	glade-cell-renderer-editor.h	\
	glade-column-types.h		\
	glade-combo-box-editor.h	\
	glade-combo-box-text-editor.h	\
	glade-entry-editor.h		\
	glade-file-chooser-button-editor.h \
	glade-file-chooser-dialog-editor.h \
	glade-file-chooser-editor.h	\
	glade-file-chooser-widget-editor.h \
	glade-fixed.h			\
	glade-font-button-editor.h 	\
	glade-font-chooser-dialog-editor.h \
	glade-font-chooser-editor.h	\
	glade-font-chooser-widget-editor.h \
	glade-grid-editor.h		\
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
	glade-header-bar-editor.h	\
	glade-icon-factory-editor.h	\
	glade-icon-sources.h		\
	glade-icon-view-editor.h	\
	glade-image-editor.h		\
	glade-image-item-editor.h	\
	glade-label-editor.h		\
	glade-layout-editor.h		\
	glade-level-bar-editor.h	\
	glade-message-dialog-editor.h	\
	glade-misc-editor.h		\
	glade-model-button-editor.h	\
	glade-model-data.h		\
	glade-notebook-editor.h		\
	glade-popover-editor.h		\
	glade-popover-menu-editor.h	\
	glade-progress-bar-editor.h 	\
	glade-real-tree-view-editor.h 	\
	glade-recent-action-editor.h	\
	glade-recent-chooser-dialog-editor.h \
	glade-recent-chooser-editor.h	\
	glade-recent-chooser-menu-editor.h \
	glade-recent-chooser-widget-editor.h \
	glade-scale-editor.h		\
	glade-scale-button-editor.h	\
	glade-scrollable-editor.h	\
	glade-scrollbar-editor.h	\
	glade-scrolled-window-editor.h	\
	glade-spin-button-editor.h	\
	glade-stack-editor.h		\
	glade-stack-switcher-editor.h	\
	glade-store-editor.h		\
	glade-string-list.h		\
	glade-text-view-editor.h	\
	glade-tool-button-editor.h	\
	glade-tool-item-group-editor.h	\
	glade-tool-palette-editor.h	\
	glade-treeview-editor.h		\
	glade-viewport-editor.h		\
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
	glade-action-editor.ui		\
	glade-activatable-editor.ui	\
	glade-app-chooser-button-editor.ui \
	glade-app-chooser-widget-editor.ui \
	glade-arrow-editor.ui		\
	glade-action-bar-editor.ui	\
	glade-box-editor.ui		\
	glade-button-editor.ui		\
	glade-combo-box-editor.ui	\
	glade-combo-box-text-editor.ui	\
	glade-entry-editor.ui		\
	glade-file-chooser-button-editor.ui \
	glade-file-chooser-dialog-editor.ui \
	glade-file-chooser-editor.ui	\
	glade-file-chooser-widget-editor.ui \
	glade-font-button-editor.ui 	\
	glade-font-chooser-dialog-editor.ui \
	glade-font-chooser-editor.ui	\
	glade-font-chooser-widget-editor.ui \
	glade-grid-editor.ui		\
	glade-header-bar-editor.ui	\
	glade-icon-view-editor.ui	\
	glade-image-editor.ui		\
	glade-label-editor.ui		\
	glade-layout-editor.ui		\
	glade-level-bar-editor.ui	\
	glade-message-dialog-editor.ui	\
	glade-misc-editor.ui		\
	glade-model-button-editor.ui	\
	glade-notebook-editor.ui	\
	glade-popover-editor.ui		\
	glade-popover-menu-editor.ui	\
	glade-progress-bar-editor.ui	\
	glade-real-tree-view-editor.ui 	\
	glade-recent-action-editor.ui	\
	glade-recent-chooser-dialog-editor.ui \
	glade-recent-chooser-editor.ui	\
	glade-recent-chooser-menu-editor.ui \
	glade-recent-chooser-widget-editor.ui \
	glade-scale-editor.ui		\
	glade-scale-button-editor.ui	\
	glade-scrollable-editor.ui	\
	glade-scrollbar-editor.ui	\
	glade-scrolled-window-editor.ui	\
	glade-spin-button-editor.ui	\
	glade-stack-editor.ui		\
	glade-stack-switcher-editor.ui	\
	glade-text-view-editor.ui	\
	glade-tool-button-editor.ui	\
	glade-tool-palette-editor.ui	\
	glade-viewport-editor.ui	\
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
