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
	glade-resources.c \
	glade-preferences.c \
	glade-http.c \
	glade-registration.c \
	main.c

noinst_HEADERS = \
	glade-window.h \
	glade-resources.h \
	glade-preferences.h \
	glade-logo.h \
	glade-registration.h \
	glade-http.h

# Generate resources
glade-resources.h: glade-resources.gresource.xml $(UI_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/glade-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _glade --generate-header
glade-resources.c: glade-resources.gresource.xml $(UI_FILES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/glade-resources.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _glade --generate-source

if NATIVE_WIN32
glade_LDADD += glade-win32-res.o

glade_LDFLAGS += -export-dynamic

if !GLADE_UNSTABLE
glade_LDFLAGS += -mwindows
endif
endif

glade-win32-res.o: glade.rc
	$(WINDRES) $< $@

UI_FILES = glade.glade glade-preferences.glade glade-registration.glade glade-registration.css

EXTRA_DIST = glade.rc.in glade-resources.gresource.xml $(UI_FILES)
