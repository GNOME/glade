SUBDIRS = po data gladeui src plugins doc help

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	autogen.sh \
	COPYING.GPL \
	COPYING.LGPL \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(NULL)

DISTCLEANFILES = intltool-extract \
                 intltool-merge \
                 intltool-update \
                 $(NULL)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
