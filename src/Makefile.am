## Process this file with automake to produce Makefile.in

bin_PROGRAMS = glade2

INCLUDES = \
	$(GLADE_CFLAGS) \
	-I$(top_srcdir)/intl \
	-I$(top_builddir)/intl

DIST_SUBDIRS = 

CFLAGS = -g -O2 -Wall

SUBDIRS = 

glade2_LDADD = \
	$(GLADE_LIBS) \
	$(INTLLIBS)

glade2_DEPENDENCIES = \
	$(INTLLIBS)

glade2_SOURCES = \
	main.c \
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
	glade-widget.c \
	glade-catalog.c \
	glade-choice.c \
	glade-editor.c \
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
	glade-types.h \
	glade-widget.h \
	glade-widget-class.h \
	glade-property.h \
	glade-property-class.h \
	glade-choice.h \
	glade-catalog.h \
	glade-xml-utils.h
