## Process this file with automake to produce Makefile.in
include $(top_srcdir)/glade-rules.mk

bin_PROGRAMS = glade

glade_CPPFLAGS = \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DGLADE_GNOMEHELPDIR="\"$(HELP_DIR)\"" \
	$(GTK_CFLAGS)      \
	$(GTK_MAC_CFLAGS)  \
	$(WARN_CFLAGS)     \
	$(AM_CPPFLAGS)

glade_CFLAGS =           \
	$(GMODULE_EXPORT_CFLAGS) \
	$(AM_CFLAGS)

glade_LDFLAGS = $(AM_LDFLAGS) $(GMODULE_EXPORT_LDFLAGS)

glade_LDADD = $(top_builddir)/gladeui/libgladeui-2.la $(GTK_MAC_LIBS)

BUILT_SOURCES = glade-resources.c glade-resources.h

glade_SOURCES = \
	glade-window.c \
	glade-window.h \
	glade-close-button.c \
	glade-close-button.h \
	glade-resources.c \
	glade-callbacks.h \
	glade-preferences.c \
	glade-preferences.h \
	main.c

noinst_HEADERS = \
	glade-window.h \
	glade-close-button.h \
	glade-resources.h \
	glade-preferences.h

# This could be split in two, but its better to rebuild both, sources and header
# each time the xml or the actual resources files change, just in case. 
glade-resources.c glade-resources.h: glade-resources.gresource.xml \
	$(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies glade-resources.gresource.xml)

if NATIVE_WIN32
glade_LDADD += glade-win32-res.o

glade_LDFLAGS += -export-dynamic

if !GLADE_UNSTABLE
glade_LDFLAGS += -mwindows
endif
endif

glade-win32-res.o: glade.rc
	$(WINDRES) $< $@

EXTRA_DIST = glade.rc.in glade-resources.gresource.xml glade.glade
