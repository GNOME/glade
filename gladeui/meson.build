gladeui_inc = include_directories('.')

common_c_flags = [
  '-DG_LOG_DOMAIN="GladeUI"',
  '-DGLADE_CATALOGSDIR="@0@"'.format(glade_prefix / glade_datadir / glade_catalogdir),
  '-DGLADE_MODULESDIR="@0@"'.format(glade_prefix / glade_libdir / glade_moduledir),
  '-DGLADE_PIXMAPSDIR="@0@"'.format(glade_prefix / glade_datadir / glade_pixmapdir),
  '-DGLADE_BINDIR="@0@"'.format(glade_prefix / glade_bindir),
  '-DGLADE_LIBDIR="@0@"'.format(glade_prefix / glade_libdir),
  '-DGLADE_LOCALEDIR="@0@"'.format(glade_prefix / glade_localedir),
]

# FIXME: should these be used in the introspection data if headers are not installed?
gir_headers = files(
  'glade-id-allocator.h',
  'glade-named-icon-chooser-dialog.h',
)

# The glade core library
headers = files(
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
  'glade-property-def.h',
  'glade-property-label.h',
  'glade-property-shell.h',
  'glade-signal.h',
  'glade-signal-def.h',
  'glade-signal-editor.h',
  'glade-signal-model.h',
  'glade-utils.h',
  'glade-widget.h',
  'glade-widget-action.h',
  'glade-widget-adaptor.h',
  'glade-xml-utils.h',
)

private_headers = files(
  'glade-drag.h',
  'glade-private.h',
  'glade-design-private.h',
)

install_headers(
  headers,
  subdir: 'lib@0@/@1@'.format(gladeui_libname, gladeui_name),
)

# Enum type generation
enum_types = 'gladeui-enum-types'

enum_sources = gnome.mkenums(
  enum_types,
  sources: headers,
  c_template: enum_types + '.c.template',
  h_template: enum_types + '.h.template',
)

marshal = 'glade-marshallers'

# Marshaller generation
marshal_sources = gnome.genmarshal(
  marshal,
  sources: marshal + '.list',
  prefix: '_glade_marshal',
  extra_args: '--prototypes',
)

sources = files(
  'glade-accumulators.c',
  'glade-adaptor-chooser.c',
  'glade-adaptor-chooser-widget.c',
  'glade-app.c',
  'glade-base-editor.c',
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
  'glade-dnd.c',
  'glade-drag.c',
  'glade-editable.c',
  'glade-editor.c',
  'glade-editor-property.c',
  'glade-editor-skeleton.c',
  'glade-editor-table.c',
  'glade-id-allocator.c',
  'glade-inspector.c',
  'glade-name-context.c',
  'glade-named-icon-chooser-dialog.c',
  'glade-object-stub.c',
  'glade-palette.c',
  'glade-placeholder.c',
  'glade-popup.c',
  'glade-preview.c',
  'glade-project.c',
  'glade-project-properties.c',
  'glade-property.c',
  'glade-property-def.c',
  'glade-property-label.c',
  'glade-property-shell.c',
  'glade-signal.c',
  'glade-signal-def.c',
  'glade-signal-editor.c',
  'glade-signal-model.c',
  'glade-template.c',
  'glade-tsort.c',
  'glade-utils.c',
  'glade-widget-action.c',
  'glade-widget-adaptor.c',
  'glade-widget.c',
  'glade-xml-utils.c',
)

# Generate resources
resource_data = files(
  'atk.png',
  'glade-adaptor-chooser.ui',
  'glade-adaptor-chooser-widget.ui',
  'glade-base-editor.ui',
  'glade-design-layout.css',
  'glade-editor.ui',
  'glade-project-properties.ui',
  'glade-property-label.ui'
)

resource = 'gladeui-resources'

sources += gnome.compile_resources(
  resource,
  resource + '.gresource.xml',
  c_name: '_gladeui',
  dependencies: resource_data,
  export: true,
)

deps = [
  glib_dep,
  gmodule_dep,
  gtk_dep,
]

private_deps = [
  libxml_dep,
  m_dep,
]

c_flags = common_flags

ldflags = []

if enable_mac_integration
  private_deps += gtk_mac_integration_gtk3_dep
  if get_option('mac_bundle')
    c_flags += '-DMAC_BUNDLE'
  endif
endif

if glade_system == 'windows'
  rc = configure_file(
    input: 'glade.rc.in',
    output: '@BASELINE@',
    configuration: rc_conf,
  )

  sources += windows.compile_resources(rc)

  ldflags += cc.get_supported_link_arguments([
    # gmodule-export-2.0 on windows does not include -export-dynamic flag
    '-export-dynamic',
    '-no-undefined',
  ])
endif

libgladeui = shared_library(
  '@0@-@1@'.format(gladeui_name, gladeui_major_version),
  version: gladeui_libversion,
  sources: sources + enum_sources + marshal_sources + private_headers + headers,
  include_directories: top_inc,
  dependencies: deps + private_deps,
  c_args: common_c_flags,
  link_args: ldflags,
  install: true,
)

libgladeui_dep = declare_dependency(
  sources: [enum_sources[1], marshal_sources[1]],
  include_directories: gladeui_inc,
  dependencies: deps,
  link_with: libgladeui,
)


variables = [
  'datadir=${prefix}' / glade_datadir,
  'datarootdir=${datadir}',
  'exec_prefix=${prefix}',
  'catalogdir=${datadir}' / glade_catalogdir,
  'moduledir=${libdir}' / glade_moduledir,
  'pixmapdir=${datadir}' / glade_pixmapdir,
]

pkg.generate(
  libgladeui,
  filebase: gladeui_libname,
  name: 'Glade',
  description: 'Glade interface designer library',
  subdirs: 'lib' + gladeui_libname,
  requires: deps,
  variables: variables,
)

# GObject Introspection
enable_introspection = get_option('introspection')
if enable_introspection
  gnome.generate_gir(
    libgladeui,
    sources: sources + enum_sources + marshal_sources + headers + gir_headers,
    includes: 'Gtk-3.0',
    nsversion: gladeui_version,
    namespace: 'Gladeui',
    identifier_prefix: 'Glade',
    symbol_prefix: glade_name,
    export_packages: gladeui_libname,
    header: 'gladeui/glade.h',
    install: true,
  )
endif

## Previewer
sources = files(
  'glade-previewer.c',
  'glade-previewer-main.c',
  'glade-preview-template.c',
)

ldflags = []
if glade_system == 'windows'
  rc = configure_file(
    input: 'glade-previewer.rc.in',
    output: '@BASELINE@',
    configuration: rc_conf,
  )

  sources += windows.compile_resources(rc)

  if not glade_unstable
    ldflags += cc.get_supported_link_arguments('-mwindows')
  endif
endif

executable(
  'glade-previewer',
  sources,
  include_directories: top_inc,
  dependencies: libgladeui_dep,
  c_args: common_c_flags + ['-DGLADE_GNOMEHELPDIR="@0@"'.format(glade_prefix / glade_datadir / 'help')],
  link_args: ldflags,
  install: true,
)
