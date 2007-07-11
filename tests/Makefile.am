## Process this file with automake to produce Makefile.in


EXTRA_DIST = \
	test-python-gladeui.py \
	python-plugin.py       \
	python-plugin.xml
	

pythonplugindir = $(pkglibdir)/modules
pythonplugin_DATA =
catalogsdir = $(pkgdatadir)/catalogs
catalogs_DATA = 

if ENABLE_PYTHON
pythonplugin_DATA += pythonplugin.py
catalogs_DATA += pythonplugin.xml
endif
