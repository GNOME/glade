## Process this file with automake to produce Makefile.in

bin_PROGRAMS = glade-3

glade_3_CPPFLAGS = \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DGLADE_GNOMEHELPDIR="\"$(HELP_DIR)\""
	$(AM_CPPFLAGS)

glade_3_CFLAGS =           \
	$(GTK_CFLAGS)      \
	$(IGE_MAC_CFLAGS)  \
	$(WARN_CFLAGS)     \
	$(AM_CFLAGS)

glade_3_LDFLAGS = $(AM_LDFLAGS)

glade_3_LDADD = $(top_builddir)/gladeui/libgladeui-1.la $(IGE_MAC_LIBS)

glade_3_SOURCES = \
	glade-window.c \
	glade-window.h \
	main.c

if NATIVE_WIN32
glade_3_LDADD += glade-win32-res.o
if !GLADE_UNSTABLE
glade_3_LDFLAGS += -mwindows
endif
endif

glade-win32-res.o: glade-3.rc
	$(WINDRES) $< $@

EXTRA_DIST = glade-3.rc.in
