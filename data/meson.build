data_dict = configuration_data()
data_dict.set('VERSION', '@0@'.format (meson.project_version()))
glade_desktop_in = configure_file(
  input: 'glade.desktop.in.in',
  output: 'glade.desktop.in',
  configuration: data_dict
)

i18n.merge_file(
  input: glade_desktop_in,
  output: 'glade.desktop',
  po_dir: join_paths(meson.source_root(), 'po'),
  type: 'desktop',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

i18n.merge_file(
  input: 'glade.appdata.xml.in',
  output: 'glade.appdata.xml',
  po_dir: join_paths(meson.source_root(), 'po'),
  type: 'xml',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

install_data(
  'gettext/its/glade-catalog.loc',
  'gettext/its/glade-catalog.its',
  install_dir: join_paths(get_option('datadir'), 'gettext', 'its'),
)

subdir('icons')
