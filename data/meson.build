subdir('icons')

desktop_conf = configuration_data()
desktop_conf.set('VERSION', glade_version)

desktop = glade_name + '.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: '@BASENAME@',
  configuration: desktop_conf,
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: glade_datadir / 'applications',
)

appdata = glade_name + '.appdata.xml'

i18n.merge_file(
  appdata,
  input: appdata + '.in',
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: glade_datadir / 'metainfo',
)

install_subdir(
  'gettext',
  install_dir: glade_datadir,
)
