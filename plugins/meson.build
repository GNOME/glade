ldflags = []
if glade_system == 'windows'
  ldflags += cc.get_supported_link_arguments('-no-undefined')
endif

plugins_kwargs = {
  'include_directories': top_inc,
  'link_args': ldflags,
  'install': true,
  'install_dir': glade_libdir / glade_moduledir,
}

subdir('gtk+')

if have_python
  subdir('python')
endif

enable_gladeui = get_option('gladeui')
if enable_gladeui
  subdir('gladeui')
endif

if have_webkit2gtk
  subdir('webkit2gtk')
endif

install_data(
  'glade-catalog.dtd',
  install_dir: glade_datadir / glade_catalogdir,
)
