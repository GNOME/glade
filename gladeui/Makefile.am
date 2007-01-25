
common_defines = \
	-DGLADE_DATADIR="\"$(pkgdatadir)\""		\
	-DGLADE_LIBDIR="\"$(pkglibdir)\""		\
	-DGLADE_CATALOGSDIR="\"$(pkgdatadir)/catalogs\""\
	-DGLADE_MODULESDIR="\"$(pkglibdir)/modules\""	\
	-DGLADE_PLUGINSDIR="\"$(pkglibdir)/plugins\""	\
	-DGLADE_BINDINGSDIR="\"$(pkglibdir)/bindings\""	\
	-DGLADE_PIXMAPSDIR="\"$(pkgdatadir)/pixmaps\""	\
	-DGLADE_SCRIPTSDIR="\"$(pkgdatadir)/scripts\""	\
	-DGLADE_LOCALEDIR="\"$(datadir)/locale\""

lib_LTLIBRARIES = libgladeui-1.la

BUILT_SOURCES = glade-marshallers.c glade-marshallers.h

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = glade-marshallers.list

# The glade-3 core library
libgladeui_1_la_SOURCES = \
	glade-widget-adaptor.c \
	glade-debug.c \
	glade-project.c \
	glade-parameter.c \
	glade-placeholder.c \
	glade-custom.c \
	glade-project-view.c \
	glade-xml-utils.c \
	glade-palette.c \
	glade-palette-item.c \
	glade-palette-box.c \
	glade-palette-expander.c \
	glade-design-layout.c \
	glade-design-view.c \
	glade-cursor.c \
	glade-property.c \
	glade-property-class.c \
	glade-popup.c \
	glade-widget.c \
	glade-catalog.c \
	glade-editor-property.c \
	glade-editor.c \
	glade-utils.c \
	glade-signal.c \
	glade-signal-editor.c \
	glade-clipboard.c \
	glade-clipboard-view.c \
	glade-command.c \
	glade-id-allocator.c \
	glade-marshallers.c \
	glade-accumulators.c \
	glade-parser.c \
	glade-builtins.c \
	glade-app.c \
	glade-keysyms.c \
	glade-fixed.c \
	glade-binding.c \
	glade-base-editor.c

libgladeui_1_la_CPPFLAGS =  \
	-DINSIDE_LIBGLADEUI \
	$(common_defines)   \
	-I$(top_srcdir)     \
	-I$(top_builddir)   \
	$(AM_CPPFLAGS)

libgladeui_1_la_CFLAGS =    \
	-g -Wall            \
	$(GTK_CFLAGS)       \
	$(AM_CFLAGS)

libgladeui_1_la_LDFLAGS = -version-info $(GLADE_CURRENT):$(GLADE_REVISION):$(GLADE_AGE) $(AM_LDFLAGS)
libgladeui_1_la_LIBADD = $(GTK_LIBS)

libgladeuiincludedir=$(includedir)/libgladeui-1.0/gladeui
libgladeuiinclude_HEADERS = \
	glade.h \
	glade-debug.h \
	glade-project.h \
	glade-project-view.h \
	glade-parameter.h \
	glade-placeholder.h \
	glade-custom.h \
	glade-editor.h \
	glade-editor-property.h \
	glade-signal-editor.h \
	glade-palette.h \
	glade-palette-item.h \
	glade-palette-box.h \
	glade-palette-expander.h \
	glade-design-layout.h \
	glade-design-view.h \
	glade-cursor.h \
	glade-widget.h \
	glade-widget-adaptor.h \
	glade-property.h \
	glade-property-class.h \
	glade-popup.h \
	glade-catalog.h \
	glade-utils.h \
	glade-signal.h \
	glade-xml-utils.h \
	glade-clipboard.h \
	glade-clipboard-view.h \
	glade-command.h \
	glade-id-allocator.h \
	glade-marshallers.h \
	glade-accumulators.h \
	glade-parser.h \
	glade-app.h \
	glade-builtins.h \
	glade-fixed.h \
	glade-binding.h \
	glade-base-editor.h \
	fixed_bg.xpm \
	atk.xpm
	

if WITH_WIN32
  libgladeui_1_la_LDFLAGS += -no-undefined
endif

#
# Marshaller generation
#
glade-marshallers.h: glade-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=glade_marshal $(srcdir)/glade-marshallers.list --header > glade-marshallers-h.tmp \
	&& mv glade-marshallers-h.tmp glade-marshallers.h \
	|| ( rm -f glade-marshallers-h.tmp && exit 1)
glade-marshallers.c: glade-marshallers.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) --prefix=glade_marshal $(srcdir)/glade-marshallers.list --body > glade-marshallers-c.tmp     \
	&& mv glade-marshallers-c.tmp glade-marshallers.c \
	|| ( rm -f glade-marshallers-c.tmp && exit 1 )
