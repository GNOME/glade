## Process this file with automake to produce Makefile.in
SUBDIRS=

bin_PROGRAMS = glade-3

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GLADE_CFLAGS) \
	-DGLADE_LIBDIR=\""$(libdir)"\" \
	-DGLADE_DATADIR=\""$(datadir)"\" \
	-DGLADE_BINDIR=\""$(bindir)"\" \
	-DGLADE_ICONDIR=\""$(datadir)/pixmaps"\" \
	-DGLADE_LOCALSTATEDIR=\""$(localstatedir)"\" \
	-DGLADE_LOCALEDIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"Glade\"

CFLAGS = -g -O2 -Wall 

glade_3_LDADD = \
	$(GLADE_LIBS)

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
	glade-choice.c \
	glade-editor.c \
	glade-gtk.c \
	glade-utils.c \
	glade-signal.c \
	glade-signal-editor.c \
	glade-clipboard.c \
	glade-clipboard-view.c \
	glade-command.c \
	glade-menu-editor.c \
	glade-keys-dialog.c \
	glade-transform.c

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
	glade-choice.h \
	glade-gtk.h \
	glade-catalog.h \
	glade-utils.h \
	glade-signal.h \
	glade-xml-utils.h \
	glade-clipboard.h \
	glade-clipboard-view.h \
	glade-command.h \
	glade-menu-editor.h \
	glade-keys-dialog.h \
	glade-transform.h
