## Process this file with automake to produce Makefile.in
SUBDIRS=

bin_PROGRAMS = glade-3

gladegtk_LTLIBRARIES = libgladegtk.la
gladegtkdir = $(modulesdir)

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = \
             glade-marshallers.list

libgladegtk_la_SOURCES = glade-gtk.c
libgladegtk_la_LDFLAGS = -module -avoid-version

if WITH_WIN32
  glade_3_LDFLAGS = glade_plugin.def
  libgladegtk_la_LDFLAGS += -no-undefined -L./ -Wl,$(bin_PROGRAMS).a $(GLADE_LIBS)
libgladegtk_la_DEPENDENCIES = $(bin_PROGRAMS).a
  CLEANFILES += $(bin_PROGRAMS).a
endif

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GLADE_CFLAGS) \
	-DMODULES_DIR=\""@modulesdir@"\"

AM_CFLAGS = -g -Wall 

glade_3_LDADD = \
	$(GLADE_LIBS)

BUILT_SOURCES = \
        glade-marshallers.c \
        glade-marshallers.h

glade_3_SOURCES = \
	main.c \
	glade-debug.c \
	glade-project.c \
	glade-parameter.c \
	glade-placeholder.c \
	glade-project-view.c \
	glade-project-window.c \
	glade-widget-class.c \
	glade-xml-utils.c \
	glade-palette.c \
	glade-cursor.c \
	glade-property.c \
	glade-property-class.c \
	glade-popup.c \
	glade-widget.c \
	glade-catalog.c \
	glade-editor.c \
	glade-utils.c \
	glade-signal.c \
	glade-signal-editor.c \
	glade-clipboard.c \
	glade-clipboard-view.c \
	glade-command.c \
	glade-menu-editor.c \
	glade-keys-dialog.c \
	glade-transform.c \
	glade-id-allocator.c \
	glade-marshallers.c

noinst_HEADERS = \
	glade.h \
	glade-debug.h \
	glade-project.h \
	glade-project-view.h \
	glade-parameter.h \
	glade-placeholder.h \
	glade-editor.h \
	glade-signal-editor.h \
	glade-palette.h \
	glade-cursor.h \
	glade-project-window.h \
	glade-types.h \
	glade-widget.h \
	glade-widget-class.h \
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
	glade-menu-editor.h \
	glade-keys-dialog.h \
	glade-transform.h \
	glade-id-allocator.h \
	glade-marshallers.h

$(bin_PROGRAMS).a: $(bin_PROGRAMS)
	$(DLLTOOL) -D $(bin_PROGRAMS) -d glade_plugin.def -l $(bin_PROGRAMS).a
#
# Marshaller generation
#
glade-marshallers.h: glade-marshallers.list $(GLIB_GENMARSHAL) Makefile.am
	$(GLIB_GENMARSHAL) --prefix=glade_marshal $(srcdir)/glade-marshallers.list --header > glade-marshallers-h.tmp \
	&& mv glade-marshallers-h.tmp glade-marshallers.h \
	|| ( rm -f glade-marshallers-h.tmp && exit 1)
glade-marshallers.c: glade-marshallers.list $(GLIB_GENMARSHAL) Makefile.am
	$(GLIB_GENMARSHAL) --prefix=glade_marshal $(srcdir)/glade-marshallers.list --body > glade-marshallers-c.tmp     \
	&& mv glade-marshallers-c.tmp glade-marshallers.c \
	|| ( rm -f glade-marshallers-c.tmp && exit 1 )
