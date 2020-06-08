envs = [
  'GLADE_TESTING=1',
  'GLADE_CATALOG_SEARCH_PATH=' + (source_root / 'plugins/gtk+'),
  'GLADE_MODULE_SEARCH_PATH=' + (build_root / 'plugins/gtk+'),
  'GLADE_PIXMAP_DIR=' + (source_root / 'data/icons'),
  'GLADE_ICON_THEME_PATH=' + (source_root / 'plugins/gtk+/icons/22x22'),
]

modenvs = [
  'GLADE_TESTING=1',
  'GLADE_CATALOG_SEARCH_PATH=' + (source_root / 'plugins/gtk+:') +
    (source_root / 'plugins/python:') + (source_root / 'plugins/gjs:') + (source_root / 'tests/catalogs'),
  'GLADE_MODULE_SEARCH_PATH=' + (build_root / 'plugins/gtk+:') +
    (build_root / 'plugins/python:') + (build_root / 'plugins/gjs:') + (source_root / 'tests/modules'),
  'GLADE_PIXMAP_DIR=' + (source_root / 'data/icons'),
  'GLADE_ICON_THEME_PATH=' + (source_root / 'plugins/gtk+/icons/22x22'),
]

test_unit = [
  ['add-child', {'sources': 'add-child.c'}, envs],
  ['create-widgets', {'sources': 'create-widgets.c'}, envs],
  ['modules', {'sources': 'modules.c'}, modenvs],
]

sources = files('toplevel-order.c')

# Generate resources
resource_data = files(
  'toplevel_order_test2.glade',
  'toplevel_order_test3.glade',
  'toplevel_order_test4.glade',
  'toplevel_order_test5.glade',
  'toplevel_order_test6.glade',
  'toplevel_order_test.glade'
)

resource = 'toplevel-order-resources'

sources += gnome.compile_resources(
  resource,
  resource + '.gresource.xml',
  c_name: '_glade',
  dependencies: resource_data,
  export: true,
)

test_unit += [['toplevel-order', {'sources': sources}, envs]]

foreach unit: test_unit
  exe = executable(
    unit[0],
    include_directories: top_inc,
    dependencies: libgladeui_dep,
    c_args: common_c_flags,
    kwargs: unit[1],
  )

  test(
    unit[0],
    exe,
    env: unit[2],
  )
endforeach
