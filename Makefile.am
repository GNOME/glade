## Process this file with automake to produce Makefile.in


SUBDIRS = po data gladeui src plugins bindings doc

if BUILD_USER_MANUAL
SUBDIRS += help
endif

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	autogen.sh \
	COPYING.GPL \
	COPYING.LGPL \
	m4 \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	gnome-doc-utils.make

DISTCLEANFILES = intltool-extract \
                 intltool-merge \
                 intltool-update \
                 gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --enable-gtk-doc
