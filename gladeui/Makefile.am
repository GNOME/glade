## Previewer

bin_PROGRAMS = glade-previewer
lib_LTLIBRARIES = libgladeui-2.la

glade_previewer_CPPFLAGS = \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DGLADE_GNOMEHELPDIR="\"$(HELP_DIR)\"" \
	$(GTK_CFLAGS)      \
	$(GTK_MAC_CFLAGS)  \
	$(WARN_CFLAGS)     \
	$(AM_CPPFLAGS)

glade_previewer_CFLAGS =   \
	$(AM_CFLAGS)

glade_previewer_LDFLAGS = $(AM_LDFLAGS)

glade_previewer_LDADD = libgladeui-2.la $(GTK_MAC_LIBS)

glade_previewer_SOURCES = \
	glade-previewer.c

if NATIVE_WIN32
glade_previewer_LDADD += glade-win32-res.o
if !GLADE_UNSTABLE
glade_previewer_LDFLAGS += -mwindows
endif
endif

glade-win32-res.o: glade-previewer.rc
	$(WINDRES) $< $@

## Rest of the UI ;)

common_defines = \
	-DG_LOG_DOMAIN=\"GladeUI\"                      \
	-DGLADE_CATALOGSDIR="\"$(pkgdatadir)/catalogs\""\
	-DGLADE_MODULESDIR="\"$(pkglibdir)/modules\""	\
	-DGLADE_PIXMAPSDIR="\"$(pkgdatadir)/pixmaps\""	\
	-DGLADE_LOCALEDIR="\"$(datadir)/locale\""\
	-DGLADE_BINDIR="\"$(bindir)\""\
	-DGLADE_LIBDIR="\"$(libdir)\""

BUILT_SOURCES = glade-marshallers.c glade-marshallers.h

EXTRA_DIST = glade-marshallers.list gladeui.rc.in icon-naming-spec.c glade-previewer.rc.in

# The glade core library
libgladeui_2_la_SOURCES = \
	glade-debug.c \
	glade-object-stub.c \
	glade-xml-utils.c \
	glade-catalog.c \
	glade-composite-template.c \
	glade-widget-adaptor.c \
	glade-widget.c \
	glade-property-class.c \
	glade-property.c \
	glade-signal-class.c \
	glade-signal.c \
	glade-widget-action.c \
	glade-editable.c \
	glade-editor-table.c \
	glade-editor-property.c \
	glade-editor.c \
	glade-base-editor.c \
	glade-signal-editor.c \
	glade-palette.c \
	glade-inspector.c \
	glade-design-layout.c \
	glade-design-view.c \
	glade-id-allocator.c \
	glade-id-allocator.h \
	glade-project.c \
	glade-cursor.c \
	glade-placeholder.c \
	glade-named-icon-chooser-dialog.c \
	glade-named-icon-chooser-dialog.h \
	glade-popup.c \
	glade-utils.c \
	glade-command.c \
	glade-marshallers.c \
	glade-accumulators.c \
	glade-builtins.c \
	glade-app.c \
	glade-clipboard.c \
	glade-name-context.c \
	glade-displayable-values.c \
	glade-signal-model.c \
	glade-cell-renderer-icon.c \
	glade-preview.c

libgladeui_2_la_CPPFLAGS =  \
	$(common_defines)   \
	-I$(top_srcdir)     \
	-I$(top_builddir)   \
	$(GTK_CFLAGS)       \
	$(GTK_MAC_BUNDLE_FLAG) \
	$(GTK_MAC_CFLAGS)  \
	$(WARN_CFLAGS)      \
	$(AM_CPPFLAGS)

libgladeui_2_la_CFLAGS =    \
	$(AM_CFLAGS)

libgladeui_2_la_LDFLAGS = -version-info $(GLADE_CURRENT):$(GLADE_REVISION):$(GLADE_AGE) $(AM_LDFLAGS)
libgladeui_2_la_LIBADD = $(GTK_LIBS) $(GTK_MAC_LIBS)


libgladeuiincludedir=$(includedir)/libgladeui-2.0/gladeui
libgladeuiinclude_HEADERS = \
	glade.h \
	glade-debug.h \
	glade-project.h \
	glade-inspector.h \
	glade-placeholder.h \
	glade-editor.h \
	glade-editor-property.h \
	glade-signal-editor.h \
	glade-palette.h \
	glade-design-view.h \
	glade-widget.h \
	glade-widget-adaptor.h \
	glade-composite-template.h \
	glade-property.h \
	glade-property-class.h \
	glade-utils.h \
	glade-clipboard.h \
	glade-command.h \
	glade-app.h \
	glade-builtins.h \
	glade-base-editor.h \
	glade-xml-utils.h \
	glade-signal.h \
	glade-signal-class.h \
	glade-catalog.h \
	glade-widget-action.h \
	glade-name-context.h \
	glade-displayable-values.h \
	glade-editable.h \
	glade-editor-table.h \
	glade-cell-renderer-icon.h \
	glade-cursor.h \
	glade-signal-model.h

noinst_HEADERS = \
	glade-marshallers.h \
	glade-design-layout.h \
	glade-object-stub.h \
	glade-popup.h \
	glade-accumulators.h \
	glade-design-private.h \
	glade-preview-tokens.h \
	glade-preview.h \
	glade-path.h

if PLATFORM_WIN32
libgladeui_2_la_LDFLAGS += -no-undefined
endif

if NATIVE_WIN32
# gmodule-export-2.0 on windows does not include -export-dynamic flag
libgladeui_2_la_LDFLAGS += \
	-Wl,gladeui-win32-res.o \
	-export-dynamic
libgladeui_2_la_DEPENDENCIES = gladeui-win32-res.o
endif

gladeui-win32-res.o: gladeui.rc
	$(WINDRES) $< $@

#
# Marshaller generation
#
glade-marshallers.h: glade-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=_glade_marshal $(srcdir)/glade-marshallers.list --header > glade-marshallers-h.tmp \
	&& mv glade-marshallers-h.tmp glade-marshallers.h \
	|| ( rm -f glade-marshallers-h.tmp && exit 1)
glade-marshallers.c: glade-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=_glade_marshal $(srcdir)/glade-marshallers.list --body --header > glade-marshallers-c.tmp     \
	&& mv glade-marshallers-c.tmp glade-marshallers.c \
	|| ( rm -f glade-marshallers-c.tmp && exit 1 )

CLEANFILES = $(BUILT_SOURCES)

#
# Introspection generation
#

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--identifier-prefix=Glade \
	--symbol-prefix=glade \
	--pkg=gtk+-3.0 -I../ -I$(srcdir)/..

INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = $(libgladeui_2_la_SOURCES) $(libgladeuiinclude_HEADERS)

Gladeui-2.0.gir: libgladeui-2.la
Gladeui_2_0_gir_INCLUDES = Gtk-3.0
Gladeui_2_0_gir_CFLAGS = $(INCLUDES)
Gladeui_2_0_gir_LIBS = libgladeui-2.la
Gladeui_2_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Gladeui-2.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
