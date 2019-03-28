deps = [
  libgladeui_dep,
  webkit2gtk_dep,
]

shared_module(
  'gladewebkit2gtk',
  sources: 'glade-webkit2gtk.c',
  dependencies: deps,
  c_args: '-DG_LOG_DOMAIN="GladeUI-WEBKIT2GTK"',
  kwargs: plugins_kwargs,
)

catalog = 'webkit2gtk.xml'

i18n.merge_file(
  catalog,
  input: catalog + '.in',
  output: '@BASENAME@',
  po_dir: po_dir,
  data_dirs: its_dir,
  install: true,
  install_dir: glade_datadir / glade_catalogdir,
)
