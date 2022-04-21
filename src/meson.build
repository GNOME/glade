fs = import('fs')

if fs.exists('../.git')
  vcs_tag(input: 'version.h.in', output: 'version.h')
else
  config = configuration_data()
  config.set ('VCS_TAG', glade_version)
  configure_file(
    input: 'version.h.in',
    output: 'version.h',
    configuration: config
  )
endif

sources = files(
  'glade-http.c',
  'glade-intro.c',
  'glade-preferences.c',
  'glade-registration.c',
  'glade-settings.c',
  'glade-window.c',
  'main.c',
)

resource_data = files(
  'glade.glade',
  'glade-preferences.glade',
  'glade-registration.css',
  'glade-registration.glade',
  'glade-window.css'
)

resource = 'glade-resources'

sources += gnome.compile_resources(
  resource,
  resource + '.gresource.xml',
  c_name: '_glade',
  dependencies: resource_data,
  export: true,
)

deps = [
  gmodule_export_dep,
  libgladeui_dep,
  libxml_dep,
]

ldflags = []
if glade_system == 'windows'
  rc = configure_file(
    input: 'glade.rc.in',
    output: 'glade.rc',
    configuration: rc_conf,
  )

  sources += windows.compile_resources(rc)

  test_ldflags = ['-Wl,--export-all-symbols']
  if not glade_unstable
    test_ldflags += ['-mwindows']
  endif
  ldflags += cc.get_supported_link_arguments(test_ldflags)
endif

executable(
  'glade',
  sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: '-DGLADE_GNOMEHELPDIR="@0@"'.format(glade_prefix / glade_datadir / 'help'),
  link_args: ldflags,
  install: true,
)
