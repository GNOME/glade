glade_resources = gnome.compile_resources(
    'glade-resources',
    'glade-resources.gresource.xml',
    source_dir: meson.current_source_dir(),
    c_name: '_glade'
)

glade_sources = files(
  'glade-window.c',
  'glade-intro.c',
  'glade-preferences.c',
  'glade-http.c',
  'glade-registration.c',
  'main.c',
)

glade_headers = files(
  'glade-window.h',
  'glade-intro.h',
  'glade-preferences.h',
  'glade-logo.h',
  'glade-registration.h',
  'glade-http.h',
)

executable(
  'glade',
  glade_sources,
  glade_headers,
  glade_resources,
  dependencies: gladeui_dep,
  install: true,
)
