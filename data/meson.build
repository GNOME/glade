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

# FIXME: replace these by install_subdir once autotools is removed
install_data(
  'icons/hicolor/scalable/apps/org.gnome.Glade.svg',
  install_dir: glade_datadir / 'icons/hicolor/scalable/apps',
)

icon_symbolic_data = files(
    'icons/hicolor/symbolic/apps/glade-brand-symbolic.svg',
    'icons/hicolor/symbolic/apps/org.gnome.Glade-symbolic.svg',
)

install_data(
  icon_symbolic_data,
  install_dir: glade_datadir / 'icons/hicolor/symbolic/apps',
)

pixmaps_data = files(
  'icons/deprecated-16x16.png',
  'icons/deprecated-22x22.png',
  'icons/devhelp.png',
  'icons/fixed-bg.png',
  'icons/placeholder.png',
  'icons/plus.png',
)

install_data(
  pixmaps_data,
  install_dir: glade_datadir / glade_pixmapdir,
)

its_data = files(
  'gettext/its/glade-catalog.its',
  'gettext/its/glade-catalog.loc',
)

install_data(
  its_data,
  install_dir: glade_datadir / 'gettext/its',
)
