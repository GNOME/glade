
lib_LTLIBRARIES = libgladeui-1.la

common_defines = \
	-DG_LOG_DOMAIN=\"GladeUI\"                      \
	-DGLADE_DATADIR="\"$(pkgdatadir)\""		\
	-DGLADE_LIBDIR="\"$(pkglibdir)\""		\
	-DGLADE_CATALOGSDIR="\"$(pkgdatadir)/catalogs\""\
	-DGLADE_MODULESDIR="\"$(pkglibdir)/modules\""	\
	-DGLADE_PLUGINSDIR="\"$(pkglibdir)/plugins\""	\
	-DGLADE_PIXMAPSDIR="\"$(pkgdatadir)/pixmaps\""	\
	-DGLADE_LOCALEDIR="\"$(datadir)/locale\""	\
	-DGLADE_BINDIR="\"$(bindir)\""

BUILT_SOURCES = glade-marshallers.c glade-marshallers.h

EXTRA_DIST = glade-marshallers.list gladeui.rc.in icon-naming-spec.c

# The glade-3 core library
libgladeui_1_la_SOURCES = \
	glade-widget-adaptor.c \
	glade-debug.c \
	glade-project.c \
	glade-parameter.c \
	glade-placeholder.c \
	glade-custom.c \
	glade-inspector.c \
	glade-object-stub.c \
	glade-xml-utils.c \
	glade-palette.c \
	glade-design-layout.c \
	glade-design-view.c \
	glade-named-icon-chooser-dialog.c \
	glade-named-icon-chooser-dialog.h \
	glade-cursor.c \
	glade-catalog.c \
	glade-property.c \
	glade-property-class.c \
	glade-popup.c \
	glade-widget.c \
	glade-editor-property.c \
	glade-editor.c \
	glade-utils.c \
	glade-signal.c \
	glade-signal-editor.c \
	glade-clipboard.c \
	glade-command.c \
	glade-id-allocator.c \
	glade-id-allocator.h \
	glade-marshallers.c \
	glade-accumulators.c \
	glade-builtins.c \
	glade-app.c \
	glade-fixed.c \
	glade-base-editor.c \
	glade-popup.h \
	glade-marshallers.h \
	glade-accumulators.h \
	glade-tsort.c \
	glade-widget-action.c \
	glade-name-context.c \
	glade-displayable-values.c \
	glade-editable.c \
	glade-editor-table.c \
	glade-cell-renderer-icon.c

libgladeui_1_la_CPPFLAGS =  \
	$(common_defines)   \
	-I$(top_srcdir)     \
	-I$(top_builddir)   \
	$(GTK_CFLAGS)       \
	$(GTK_MAC_BUNDLE_FLAG) \
	$(GTK_MAC_CFLAGS)  \
	$(WARN_CFLAGS)      \
	$(AM_CPPFLAGS)

libgladeui_1_la_CFLAGS =    \
	$(AM_CFLAGS)

libgladeui_1_la_LDFLAGS = -version-info $(GLADE_CURRENT):$(GLADE_REVISION):$(GLADE_AGE) $(AM_LDFLAGS)
libgladeui_1_la_LIBADD = $(GTK_LIBS) $(GTK_MAC_LIBS)


libgladeuiincludedir=$(includedir)/libgladeui-1.0/gladeui
libgladeuiinclude_HEADERS = \
	glade.h \
	glade-debug.h \
	glade-project.h \
	glade-inspector.h \
	glade-parameter.h \
	glade-placeholder.h \
	glade-custom.h \
	glade-editor.h \
	glade-editor-property.h \
	glade-signal-editor.h \
	glade-palette.h \
	glade-design-layout.h \
	glade-design-view.h \
	glade-widget.h \
	glade-widget-adaptor.h \
	glade-property.h \
	glade-property-class.h \
	glade-utils.h \
	glade-clipboard.h \
	glade-command.h \
	glade-app.h \
	glade-builtins.h \
	glade-fixed.h \
	glade-base-editor.h \
	glade-xml-utils.h \
	glade-signal.h \
	glade-cursor.h \
	glade-catalog.h \
	glade-widget-action.h \
	glade-name-context.h \
	glade-displayable-values.h \
	glade-editable.h \
	glade-editor-table.h \
	glade-cell-renderer-icon.h


noinst_HEADERS = \
	glade-marshallers.h \
	glade-design-layout.h \
	glade-object-stub.h \
	glade-popup.h \
	glade-accumulators.h \
	glade-tsort.h \
	glade-widget-private.h

if PLATFORM_WIN32
libgladeui_1_la_LDFLAGS += -no-undefined
endif

if NATIVE_WIN32
libgladeui_1_la_LDFLAGS += -Wl,gladeui-win32-res.o -export-symbols gladeui.def
libgladeui_1_la_DEPENDENCIES = gladeui-win32-res.o gladeui.def
endif

gladeui-win32-res.o: gladeui.rc
	$(WINDRES) $< $@

# gladeui.def: auto-generated DEF file for linker input
# The routines below will scan the header files and extracts symbol declarations.
# The symbol 'custom_get_type' is added manually. It's naming deviates from
# the pattern set by all other public symbols in the glade namespace.
#
gladeui.def: $(libgladeuiinclude_HEADERS)
	@echo Generating $@
	@(echo EXPORTS; \
	echo 'custom_get_type'; \
	(cd $(srcdir); cat $(libgladeuiinclude_HEADERS) || echo 'glade_ERROR ()' ) | \
	grep -v -E '^# *include' | \
	grep -v -E '^# *define' | \
	grep -E '[^\(]glade_.*[[:space:]]+\(.*' | \
	sed -e 's/[ 	]*(.*//' | \
	sed -e 's/.*glade_/glade_/' | \
	sort; \
	) >$@
	@ ! grep -q glade_ERROR $@ || ($(RM) $@; false)

#
# Marshaller generation
#
glade-marshallers.h: glade-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=glade_marshal $(srcdir)/glade-marshallers.list --header > glade-marshallers-h.tmp \
	&& mv glade-marshallers-h.tmp glade-marshallers.h \
	|| ( rm -f glade-marshallers-h.tmp && exit 1)
glade-marshallers.c: glade-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=glade_marshal $(srcdir)/glade-marshallers.list --body --header > glade-marshallers-c.tmp     \
	&& mv glade-marshallers-c.tmp glade-marshallers.c \
	|| ( rm -f glade-marshallers-c.tmp && exit 1 )


CLEANFILES = $(BUILT_SOURCES) gladeui.def
