## Process this file with automake to produce Makefile.in

bin_PROGRAMS = glade

glade_CPPFLAGS = \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DGLADE_GNOMEHELPDIR="\"$(HELP_DIR)\""
	$(AM_CPPFLAGS)

glade_CFLAGS =           \
	$(GTK_CFLAGS)      \
	$(IGE_MAC_CFLAGS)  \
	$(WARN_CFLAGS)     \
	$(AM_CFLAGS)

glade_LDFLAGS = $(AM_LDFLAGS)

glade_LDADD = $(top_builddir)/gladeui/libgladeui-3.la $(IGE_MAC_LIBS)

glade_SOURCES = \
	glade-window.c \
	glade-window.h \
	glade-close-button.c \
	glade-close-button.h \
	main.c

if NATIVE_WIN32
glade_LDADD += glade-win32-res.o
if !GLADE_UNSTABLE
glade_LDFLAGS += -mwindows
endif
endif

glade-win32-res.o: glade.rc
	$(WINDRES) $< $@

EXTRA_DIST = glade.rc.in
