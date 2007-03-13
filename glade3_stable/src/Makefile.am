## Process this file with automake to produce Makefile.in

bin_PROGRAMS = glade-3

glade_3_CPPFLAGS = \
	-I$(top_srcdir)    \
	-I$(top_builddir)  \
	-DGLADE_GNOMEHELPDIR="\"$(HELP_DIR)\""
	$(AM_CPPFLAGS)

glade_3_CFLAGS =            \
	-g @GCC_WARN_FLAGS@ \
	$(GTK_CFLAGS)       \
	$(AM_CFLAGS)

glade_3_LDFLAGS = $(AM_LDFLAGS)

glade_3_LDADD = $(top_builddir)/gladeui/libgladeui-1.la

glade_3_SOURCES = \
	glade-project-window.c \
	glade-project-window.h \
	main.c

if WITH_WIN32
  glade_3_LDFLAGS += -mwindows
endif
