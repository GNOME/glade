deps = [
  libgladeui_dep,
  pygobject_dep,
  python_dep,
]

shared_module(
  '@0@gladepython'.format(module_prefix),
  sources: 'glade-python.c',
  dependencies: deps,
  c_args: '-DG_LOG_DOMAIN="GladeUI-PYTHON"',
  kwargs: plugins_kwargs,
)
