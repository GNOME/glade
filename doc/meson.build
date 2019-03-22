version_xml_data = configuration_data()
version_xml_data.set('PACKAGE_STRING', '@0@ @1@'.format (meson.project_name(), meson.project_version()))
version_xml = configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: version_xml_data
)

gnome.gtkdoc(
  'gladeui',
  main_sgml: 'gladeui-docs.sgml',
  content_files: [
    version_xml,
    'catalogintro.sgml',
    'widgetclasses.sgml',
    'properties.sgml',
    'gladepython.sgml',
  ],
  ignore_headers: [
    'glade-builtins.h',
    'glade-debug.h',
    'glade-accumulators.h',
    'glade-marshallers.h',
    'glade-paths.h',
    'glade-custom.h',
    'glade-cursor.h',
    'glade-id-allocator.h',
    'glade-catalog.h',
    'glade.h',
    'glade-design-layout.h',
    'glade-popup.h',
    'glade-gtk.h',
    'glade-palette-expander.h',
    'glade-palette-item.h',
    'glade-named-icon-chooser-dialog.h',
    'glade-palette-box.h',
  ],
  src_dir: join_paths(meson.source_root(), 'gladeui'),
  dependencies: gladeui_deps,
)
