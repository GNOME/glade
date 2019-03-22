common_c_args = [
  '-DG_LOG_DOMAIN="GladeUI"',
  '-DGLADE_CATALOGSDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), meson.project_name(), 'catalogs')),
  '-DGLADE_MODULESDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('libdir'), meson.project_name(), 'modules')),
  '-DGLADE_PIXMAPSDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), meson.project_name(), 'pixmaps')),
  '-DGLADE_LOCALEDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'locale')),
  '-DGLADE_BINDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('bindir'))),
  '-DGLADE_LIBDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('libdir'))),
]

glade_marshallers = gnome.genmarshal(
  'glade-marshallers',
  sources: files('glade-marshallers.list'),
  prefix: '_glade_marshal',
)

gladeui_resources = gnome.compile_resources(
    'gladeui-resources',
    'gladeui-resources.gresource.xml',
    source_dir: meson.current_source_dir(),
    c_name: '_gladeui'
)

gladeui_sources = files(
  'glade-accumulators.c',
  'glade-app.c',
  'glade-base-editor.c',
  'glade-adaptor-chooser-widget.c',
  'glade-adaptor-chooser.c',
  'glade-builtins.c',
  'glade-catalog.c',
  'glade-cell-renderer-icon.c',
  'glade-clipboard.c',
  'glade-command.c',
  'glade-cursor.c',
  'glade-debug.c',
  'glade-design-layout.c',
  'glade-design-view.c',
  'glade-displayable-values.c',
  'glade-editable.c',
  'glade-editor.c',
  'glade-editor-property.c',
  'glade-editor-skeleton.c',
  'glade-editor-table.c',
  'glade-id-allocator.c',
  'glade-id-allocator.h',
  'glade-object-stub.c',
  'glade-inspector.c',
  'glade-name-context.c',
  'glade-named-icon-chooser-dialog.c',
  'glade-named-icon-chooser-dialog.h',
  'glade-palette.c',
  'glade-placeholder.c',
  'glade-popup.c',
  'glade-preview.c',
  'glade-project.c',
  'glade-project-properties.c',
  'glade-property.c',
  'glade-property-class.c',
  'glade-property-label.c',
  'glade-property-shell.c',
  'glade-signal.c',
  'glade-signal-class.c',
  'glade-signal-editor.c',
  'glade-signal-model.c',
  'glade-template.c',
  'glade-tsort.c',
  'glade-utils.c',
  'glade-widget.c',
  'glade-widget-action.c',
  'glade-widget-adaptor.c',
  'glade-xml-utils.c',
  'glade-drag.c',
  'glade-dnd.c',
)

gladeui_headers = files(
  'glade.h',
  'glade-app.h',
  'glade-adaptor-chooser.h',
  'glade-base-editor.h',
  'glade-builtins.h',
  'glade-catalog.h',
  'glade-cell-renderer-icon.h',
  'glade-clipboard.h',
  'glade-command.h',
  'glade-cursor.h',
  'glade-debug.h',
  'glade-design-view.h',
  'glade-displayable-values.h',
  'glade-editable.h',
  'glade-editor.h',
  'glade-editor-property.h',
  'glade-editor-skeleton.h',
  'glade-editor-table.h',
  'glade-inspector.h',
  'glade-name-context.h',
  'glade-palette.h',
  'glade-placeholder.h',
  'glade-project.h',
  'glade-property.h',
  'glade-property-class.h',
  'glade-property-label.h',
  'glade-property-shell.h',
  'glade-signal.h',
  'glade-signal-class.h',
  'glade-signal-editor.h',
  'glade-signal-model.h',
  'glade-utils.h',
  'glade-widget.h',
  'glade-widget-action.h',
  'glade-widget-adaptor.h',
  'glade-xml-utils.h',
)

gladeui_headers_noninst = files(
  'glade-accumulators.h',
  'glade-adaptor-chooser-widget.h',
  'glade-design-layout.h',
  'glade-design-private.h',
  'glade-object-stub.h',
  'glade-path.h',
  'glade-popup.h',
  'glade-preview.h',
  'glade-preview-tokens.h',
  'glade-previewer.h',
  'glade-preview-template.h',
  'glade-private.h',
  'glade-project-properties.h',
  'glade-tsort.h',
  'glade-drag.h',
  'glade-dnd.h',
)

gladeui_enum_types = gnome.mkenums_simple(
  'gladeui-enum-types',
  sources : gladeui_headers
)

gladeui_deps = [
  glib_dep,
  gtk_dep,
  libxml_dep,
  gmodule_dep,
  m_dep,
]

gladeui_library = library(
  'gladeui',
  gladeui_sources,
  gladeui_headers,
  gladeui_headers_noninst,
  glade_marshallers,
  gladeui_resources,
  gladeui_enum_types,
  config_header,
  c_args: [
    common_c_args,
  ],
  dependencies: gladeui_deps,
  install: true,
  include_directories: root_directory,
)

gladeui_dep = declare_dependency(
  link_with: gladeui_library,
  sources: [
    glade_marshallers,
    gladeui_resources,
    gladeui_enum_types,
    config_header,
  ],
  dependencies: gladeui_deps,
  include_directories: [
    root_directory,
    include_directories('.'),
  ],
)

install_headers(
  gladeui_headers,
  subdir: join_paths('libgladeui-2.0', 'gladeui')
)

pkg.generate(
  gladeui_library,
  filebase: 'gladeui-2.0',
  name: 'Glade',
  description: 'Glade interface designer library',
  url: 'http://glade.gnome.org',
  subdirs: 'libgladeui-2.0',
  variables: [
    'catalogdir=${datadir}/@0@/catalogs'.format (meson.project_name()),
    'pixmapdir=${datadir}/@0@/pixmaps'.format (meson.project_name()),
    'moduledir=${libdir}/@0@/modules'.format (meson.project_name()),
  ],
  requires: [
    glib_dep,
    gtk_dep,
    libxml_dep,
  ]
)

executable(
  'glade-previewer',
  'glade-previewer-main.c',
  'glade-previewer.c',
  'glade-preview-template.c',
  dependencies: gladeui_dep,
  install: true,
)
