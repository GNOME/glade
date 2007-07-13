BUILDDIR = $(top_builddir)/bindings/python/docs

REF_VERSION = $(GLADE_MAJOR_VERSION).$(GLADE_MINOR_VERSION)
FULL_VERSION = $(REF_VERSION).$(GLADE_MICRO_VERSION)

HTML_STYLE = $(PYGOBJECT_DATADIR)/pygobject/xsl/ref-html-style.xsl

XMLFILES = 				\
	pygladeui.xml			\
	pygladeui-introduction.xml	\
	pygladeui-classes.xml


HTML_FILES = 	\
	html/index.html		\
	html/index.sgml		\
	html/pygladeui.devhelp	\
	html/pygladeui-introduction.html

CSS_FILES = $(PYGOBJECT_PYGDOCS)/style.css

TARGET_DIR = $(datadir)/gtk-doc/html/pygladeui
CSSdir = $(TARGET_DIR)
CSS_DATA = $(CSS_FILES)


EXTRA_DIST = $(HTML_FILES) $(XMLFILES) 		

install-data-local:
	installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; \
	then echo '-- Nothing to install' ; \
	else \
	  $(mkinstalldirs) $(DESTDIR)$(TARGET_DIR); \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR) || :; \
	fi

install-data-hook:
	@$(PYGOBJECT_FIXXREF) $(DESTDIR)$(TARGET_DIR)

uninstall-local:
	rm -f $(DESTDIR)$(TARGET_DIR)/*

if ENABLE_PYTHON_DOCS

BUILT_SOURCES =  builddate.xml $(HTML_FILES)
DISTCLEANFILES = builddate.xml $(HTML_FILES)

builddate.xml:
	@$(PYTHON) -c 'import datetime; print datetime.date.today()' > $@

# Do not add a builddate.xml dependency here, because the generated html 
# documentation will be included in the tarball so xsltproc is not needed to 
# install the documentation. Adding a builddate.xml dependency will force 
# the documentation to be rebuilt when it doesn't need to be.
$(HTML_FILES): $(XMLFILES)
	@xsltproc --nonet --xinclude -o $(BUILDDIR)/html/ \
		  --path $(BUILDDIR):$(srcdir) \
                  --stringparam gtkdoc.bookname "pygladeui" \
                  --stringparam gtkdoc.version ${REF_VERSION} \
                  $(HTML_STYLE) $(srcdir)/pygladeui.xml

endif

