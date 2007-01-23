## Process this file with automake to produce Makefile.in

if DISABLE_USER_MANUAL
SUBDIRS = po data gladeui src plugins bindings doc
else
SUBDIRS = po data gladeui src plugins bindings help doc
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
