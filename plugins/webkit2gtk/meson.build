deps = [
  libgladeui_dep,
  webkit2gtk_dep,
]

shared_module(
  '@0@gladewebkit2gtk'.format(module_prefix),
  sources: 'glade-webkit2gtk.c',
  dependencies: deps,
  c_args: '-DG_LOG_DOMAIN="GladeUI-WEBKIT2GTK"',
  kwargs: plugins_kwargs,
)

install_data(
  'webkit2gtk.xml',
  install_dir: glade_datadir / glade_catalogdir,
)
