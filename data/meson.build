subdir('icons')

desktop_conf = configuration_data()
desktop_conf.set('VERSION', glade_version)

i18n.merge_file(
  'desktop',
  type: 'desktop',
  input: configure_file(
    input: 'org.gnome.Glade.desktop.in.in',
    output: '@BASENAME@',
    configuration: desktop_conf,
  ),
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: glade_datadir / 'applications',
)

appdata = glade_name + '.appdata.xml'

i18n.merge_file(
  'appdata',
  input: 'org.gnome.Glade.appdata.xml.in',
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: glade_datadir / 'metainfo',
)

install_subdir(
  'gettext',
  install_dir: glade_datadir,
)
