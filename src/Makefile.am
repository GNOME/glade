## Process this file with automake to produce Makefile.in
SUBDIRS=tests

bin_PROGRAMS = glade2

INCLUDES = \
	$(GLADE_CFLAGS) \
	-I$(top_srcdir)/intl \
	-I$(top_builddir)/intl

CFLAGS = -g -O2 -Wall

glade2_LDADD = \
	$(GLADE_LIBS) \
	$(INTLLIBS) \
	-lpopt
## FIXME popt should be added in configure.in and checked for

glade2_DEPENDENCIES = \
	$(INTLLIBS)

glade2_SOURCES = \
	main.c \
	glade-project.c \
	glade-parameter.c \
	glade-placeholder.c \
	glade-project-view.c \
	glade-project-window.c \
	glade-project-ui.c \
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
	glade-packing.c \
	glade-signal-editor.c

noinst_HEADERS = \
	glade.h \
	glade-project.h \
	glade-project-view.h \
	glade-parameter.h \
	glade-placeholder.h \
	glade-editor.h \
	glade-signal-editor.h \
	glade-palette.h \
	glade-cursor.h \
	glade-project-window.h \
	glade-project-ui.h \
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
	glade-packing.h \
	glade-xml-utils.h
