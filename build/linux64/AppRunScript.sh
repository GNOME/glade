#!/bin/sh

# Base environment variables
LD_LIBRARY_PATH=${APP_IMAGE_ROOT}/usr/lib:${LD_LIBRARY_PATH}
PATH=${APP_IMAGE_ROOT}/usr/bin:${PATH}
XDG_DATA_DIRS=${APP_IMAGE_ROOT}/usr/share:${XDG_DATA_DIRS}
export LD_LIBRARY_PATH PATH XDG_DATA_DIRS

# Pango environment variables
PANGO_RC_FILE=${APP_IMAGE_ROOT}/pangorc
export PANGO_RC_FILE

# GTK+/GIO/GdkPixbuf environment variables
# http://askubuntu.com/questions/251712/how-can-i-install-a-gsettings-schema-without-root-privileges
GSETTINGS_SCHEMA_DIR=${APP_IMAGE_ROOT}/usr/share/glib-2.0/schemas/:${GSETTINGS_SCHEMA_DIR}
GDK_PIXBUF_MODULE_FILE=${APP_IMAGE_ROOT}/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache
GTK_PATH=${APP_IMAGE_ROOT}/usr/lib/gtk-3.0
GTK_DATA_PREFIX=${APP_IMAGE_ROOT}/usr
GTK_THEME=Adwaita
export GSETTINGS_SCHEMA_DIR GDK_PIXBUF_MODULE_FILE GTK_PATH GTK_DATA_PREFIX GTK_THEME

# Glade environment variables
GLADE_CATALOG_SEARCH_PATH=${APP_IMAGE_ROOT}/usr/share/glade/catalogs
GLADE_MODULE_SEARCH_PATH=${APP_IMAGE_ROOT}/usr/lib/glade/modules
GLADE_PIXMAP_DIR=${APP_IMAGE_ROOT}/usr/share/glade/pixmaps
GLADE_BUNDLED=1
export GLADE_CATALOG_SEARCH_PATH GLADE_MODULE_SEARCH_PATH GLADE_PIXMAP_DIR GLADE_BUNDLED

# Invoke Glade with the arguments passed
${APP_IMAGE_ROOT}/usr/bin/glade $*
