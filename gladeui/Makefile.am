## Previewer
include $(top_srcdir)/glade-rules.mk

bin_PROGRAMS = glade-previewer
lib_LTLIBRARIES = libgladeui-2.la

glade_previewer_CPPFLAGS = \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DGLADE_GNOMEHELPDIR="\"$(HELP_DIR)\"" \
	$(GTK_CFLAGS)      \
	$(GTK_MAC_CFLAGS)  \
	$(WARN_CFLAGS)     \
	$(GLADE_DEBUG_FLAGS) \
	$(AM_CPPFLAGS)

glade_previewer_CFLAGS =   \
	$(AM_CFLAGS)

glade_previewer_LDFLAGS = $(AM_LDFLAGS)

glade_previewer_LDADD = libgladeui-2.la $(GTK_MAC_LIBS)

glade_previewer_SOURCES = \
	glade-previewer-main.c \
	glade-previewer.c \
	glade-preview-template.c

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

BUILT_SOURCES = \
	glade-marshallers.c \
	glade-marshallers.h \
	gladeui-resources.c \
	gladeui-resources.h \
	gladeui-enum-types.c \
	gladeui-enum-types.h


UI_FILES = 				\
	glade-design-layout.css		\
	glade-editor.ui			\
	glade-project-properties.ui	\
	glade-adaptor-chooser-widget.ui	\
	glade-property-label.ui

GRAPHICS_FILES = \
	atk.png


EXTRA_DIST = 			\
	$(UI_FILES)		\
	$(GRAPHICS_FILES)	\
	glade-marshallers.list 	\
	gladeui.rc.in 		\
	icon-naming-spec.c 	\
	glade-previewer.rc.in 	\
	gladeui-resources.gresource.xml \
	gladeui-enum-types.c.template	\
	gladeui-enum-types.h.template


# The glade core library
libgladeui_2_la_SOURCES = \
	$(BUILT_SOURCES)	\
	glade-accumulators.c \
	glade-app.c \
	glade-base-editor.c \
	glade-adaptor-chooser-widget.c \
	glade-builtins.c \
	glade-catalog.c \
	glade-cell-renderer-icon.c \
	glade-clipboard.c \
	glade-command.c \
	glade-cursor.c \
	glade-debug.c \
	glade-design-layout.c \
	glade-design-view.c \
	glade-displayable-values.c \
	glade-editable.c \
	glade-editor.c \
	glade-editor-property.c \
	glade-editor-skeleton.c \
	glade-editor-table.c \
	glade-id-allocator.c \
	glade-id-allocator.h \
	glade-object-stub.c \
	glade-inspector.c \
	glade-name-context.c \
	glade-named-icon-chooser-dialog.c \
	glade-named-icon-chooser-dialog.h \
	glade-palette.c \
	glade-placeholder.c \
	glade-popup.c \
	glade-preview.c	\
	glade-project.c \
	glade-project-properties.c \
	glade-property.c \
	glade-property-class.c \
	glade-property-label.c \
	glade-property-shell.c \
	glade-signal.c \
	glade-signal-class.c \
	glade-signal-editor.c \
	glade-signal-model.c \
	glade-tsort.c \
	glade-utils.c \
	glade-widget.c \
	glade-widget-action.c \
	glade-widget-adaptor.c \
	glade-xml-utils.c \
	glade-drag.c \
	glade-dnd.c

libgladeui_2_la_CPPFLAGS =  \
	$(common_defines)   \
	-I$(top_srcdir)     \
	-I$(top_builddir)   \
	$(GTK_CFLAGS)       \
	$(GTK_MAC_BUNDLE_FLAG) \
	$(GTK_MAC_CFLAGS)  \
	$(WARN_CFLAGS)      \
	$(GLADE_DEBUG_FLAGS) \
	$(AM_CPPFLAGS)

libgladeui_2_la_CFLAGS =    \
	$(AM_CFLAGS)

libgladeui_2_la_LDFLAGS = -version-info $(GLADE_CURRENT):$(GLADE_REVISION):$(GLADE_AGE) $(AM_LDFLAGS)
libgladeui_2_la_LIBADD = $(GTK_LIBS) $(GTK_MAC_LIBS) $(LIBM)

libgladeuiincludedir=$(includedir)/libgladeui-2.0/gladeui
libgladeuiinclude_HEADERS = \
	glade.h \
	glade-app.h \
	glade-base-editor.h \
	glade-builtins.h \
	glade-catalog.h \
	glade-cell-renderer-icon.h \
	glade-clipboard.h \
	glade-command.h \
	glade-cursor.h \
	glade-debug.h \
	glade-design-view.h \
	glade-displayable-values.h \
	glade-editable.h \
	glade-editor.h \
	glade-editor-property.h \
	glade-editor-skeleton.h \
	glade-editor-table.h \
	glade-inspector.h \
	glade-name-context.h \
	glade-palette.h \
	glade-placeholder.h \
	glade-project.h \
	glade-property.h \
	glade-property-class.h \
	glade-property-label.h \
	glade-property-shell.h \
	glade-signal.h \
	glade-signal-class.h \
	glade-signal-editor.h \
	glade-signal-model.h \
	glade-utils.h \
	glade-widget.h \
	glade-widget-action.h \
	glade-widget-adaptor.h \
	glade-xml-utils.h

noinst_HEADERS = \
	glade-accumulators.h \
	glade-adaptor-chooser-widget.h \
	glade-design-layout.h \
	glade-design-private.h \
	glade-marshallers.h \
	glade-object-stub.h \
	glade-path.h \
	glade-popup.h \
	glade-preview.h \
	glade-preview-tokens.h \
	glade-previewer.h \
	glade-preview-template.h \
	glade-private.h \
	glade-project-properties.h \
	glade-tsort.h \
	gladeui-resources.h \
	glade-drag.h \
	glade-dnd.h

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

#
# Enum type generation
#
gladeui-enum-types.h: gladeui-enum-types.h.template $(libgladeuiinclude_HEADERS) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template $@.template \
	$(libgladeuiinclude_HEADERS)) > $@

gladeui-enum-types.c: gladeui-enum-types.c.template $(libgladeuiinclude_HEADERS) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template $@.template \
	$(libgladeuiinclude_HEADERS)) > $@

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

# Generate resources
gladeui-resources.h: gladeui-resources.gresource.xml $(UI_FILES) $(GRAPHICS_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/gladeui-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _gladeui --generate-header
gladeui-resources.c: gladeui-resources.gresource.xml $(UI_FILES) $(GRAPHICS_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/gladeui-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _gladeui --generate-source
