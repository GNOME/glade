## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=gladeui

DOC_MODULE_VERSION=2

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
DOC_SOURCE_DIR=../gladeui

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
# XXX We'll maybe want to use this to cross-ref into GTK+ docs...
FIXXREF_OPTIONS=

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=$(top_srcdir)/gladeui/*.h
CFILE_GLOB=$(top_srcdir)/gladeui/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=\
	glade-builtins.h \
	glade-debug.h \
	glade-accumulators.h \
	glade-marshallers.h \
	glade-paths.h \
	glade-custom.h \
	glade-cursor.h \
	glade-id-allocator.h \
	glade.h \
	glade-design-layout.h \
	glade-popup.h \
	glade-gtk.h \
	glade-palette-expander.h \
	glade-palette-item.h \
	glade-named-icon-chooser-dialog.h \
	glade-palette-box.h

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=\
	version.xml \
	catalogintro.sgml \
	widgetclasses.sgml \
	properties.sgml \
	gladepython.sgml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=\
	catalogintro.sgml \
	widgetclasses.sgml \
	properties.sgml \
	gladepython.sgml

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
GTKDOC_CFLAGS= \
	-I$(top_srcdir)/gladeui	  \
	-I$(top_builddir)/gladeui \
	-I$(top_srcdir)           \
	-I$(top_builddir)         \
	`pkg-config --cflags gtk+-3.0`

GTKDOC_LIBS= `pkg-config --libs gtk+-3.0` ${top_builddir}/gladeui/libgladeui-2.la

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += version.xml.in


# XXX Seems this is no longer working with gtkdoc 1.9 :(
GTK_HOME = http://library.gnome.org/devel/gtk/stable
GDK_HOME = http://library.gnome.org/devel/gdk/stable
GOBJECT_HOME = http://library.gnome.org/devel/gobject/stable
GLIB_HOME = http://library.gnome.org/devel/glib/stable

webdocs: all
	rm -rf html-web
	mkdir -p html-web
	for file in `find html`; do \
	  ROOT_PATH=`echo $${file} | sed 's/^html//'`; \
	  if test "" != "$${ROOT_PATH}"; then \
	    if test -d $$file; then \
	      echo "Creating directory html-web$${ROOT_PATH}..."; \
	      mkdir -p html-web$${ROOT_PATH}; \
	    else \
	      if echo $$file | grep -q '\.html$$'; then \
	        echo "Creating file html-web$${ROOT_PATH}..."; \
	        cat $$file | sed \
	          -e 's!href="../gtk!href="$(GTK_HOME)!g' \
	          -e 's!href="../gdk!href="$(GDK_HOME)!g' \
	          -e 's!href="../gobject!href="$(GOBJECT_HOME)!g' \
	          -e 's!href="../glib!href="$(GLIB_HOME)!g' \
	          > html-web$${ROOT_PATH}; \
	      else \
	        echo "Copying file html-web$${ROOT_PATH}..."; \
	        cp $$file html-web$${ROOT_PATH}; \
	      fi \
	    fi; \
	  fi; \
	done
