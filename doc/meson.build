private_doc_headers = [
  'glade-builtins.h',
  'glade-debug.h',
  'glade-design-private.h',
  'glade-drag.h',
  'glade-accumulators.h',
  'glade-marshallers.h',
  'glade-paths.h',
  'glade-custom.h',
  'glade-cursor.h',
  'glade-id-allocator.h',
  'glade.h',
  'glade-design-layout.h',
  'glade-popup.h',
  'glade-gtk.h',
  'glade-palette-expander.h',
  'glade-palette-item.h',
  'glade-named-icon-chooser-dialog.h',
  'glade-palette-box.h',
  'glade-private.h',
]

content_files = files(
  'catalogintro.sgml',
  'gladepython.sgml',
  'properties.sgml',
  'widgetclasses.sgml',
)

version_conf = configuration_data()
version_conf.set('PACKAGE_STRING', '@0@ @1@'.format(glade_name, glade_version))

content_files += configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: version_conf,
)

gnome.gtkdoc(
  gladeui_name,
  main_xml: gladeui_name + '-docs.xml',
  module_version: gladeui_major_version.to_string(),
  src_dir: gladeui_inc,
  ignore_headers: private_doc_headers,
  include_directories: top_inc,
  dependencies: libgladeui_dep,
  content_files: content_files,
  install: true,
)
