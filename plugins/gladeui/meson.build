sources = files(
  'glade-glade-editor-skeleton.c',
  'glade-glade-property-shell.c',
)

shared_module(
  'gladeglade',
  sources: sources,
  dependencies: libgladeui_dep,
  c_args: '-DG_LOG_DOMAIN="GladeUI-Glade"',
  kwargs: plugins_kwargs,
)

install_data(
  'gladeui.xml',
  install_dir: glade_datadir / glade_catalogdir,
)
