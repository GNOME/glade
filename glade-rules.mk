# In this file you will find generic and usefull rules to

# GResource rules:
# These rules will create source and header files for any file ending with .gresource.xml
# You will have to manually load the resourse unless the file name ends with
# .static.gresource.xml in which case it will be loaded automatically

%.h: %.gresource.xml
	$(GLIB_COMPILE_RESOURCES) --manual-register --generate $< --target=$@
%.c: %.gresource.xml
	$(GLIB_COMPILE_RESOURCES) --manual-register --generate $< --target=$@

# rule for static resources
%.h: %.static.gresource.xml
	$(GLIB_COMPILE_RESOURCES) --generate $< --target=$@
%.c: %.static.gresource.xml
	$(GLIB_COMPILE_RESOURCES) --generate $< --target=$@
