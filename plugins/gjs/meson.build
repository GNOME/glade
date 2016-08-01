deps = [
  libgladeui_dep,
  gjs_dep,
]

shared_module(
  'gladegjs',
  sources: 'glade-gjs.c',
  dependencies: deps,
  c_args: '-DG_LOG_DOMAIN="GladeUI-GJS"',
  kwargs: plugins_kwargs,
)
