project(
  'glade', 'c',
  version: '3.22.0',
  license: 'LGPL2',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.49.0',
)

glade_name = meson.project_name()

glade_version = meson.project_version()
version_array = glade_version.split('.')
glade_major_version = version_array[0].to_int()
glade_minor_version = version_array[1].to_int()
glade_micro_version = version_array[2].to_int()

glade_prefix = get_option('prefix')
glade_bindir = get_option('bindir')
glade_datadir = get_option('datadir')
glade_libdir = get_option('libdir')
glade_localedir = get_option('localedir')
glade_mandir = get_option('mandir')

glade_catalogdir = glade_name / 'catalogs'
glade_moduledir = glade_name / 'modules'
glade_pixmapdir = glade_name / 'pixmaps'

gladeui_name = 'gladeui'

gladeui_version = '2.0'
version_array = gladeui_version.split('.')
gladeui_major_version = version_array[0].to_int()
gladeui_minor_version = version_array[1].to_int()

gladeui_libname = '@0@-@1@'.format(gladeui_name, gladeui_version)

gladeui_revision = 0
gladeui_current = 11
gladeui_age = 5
gladeui_current_minus_age = gladeui_current - gladeui_age
gladeui_libversion = '@0@.@1@.@2@'.format(gladeui_current_minus_age, gladeui_age, gladeui_revision)

glade_system = host_machine.system()
glade_debug = get_option('buildtype').contains('debug')
if glade_debug
  add_project_arguments('-DGLADE_ENABLE_DEBUG', language : 'c')
endif

glade_unstable = glade_minor_version.is_odd()

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')
windows = import('windows')

source_root = meson.current_source_dir()
build_root = meson.current_build_dir()

its_dir = source_root / 'data/gettext'
po_dir = source_root / 'po'

top_inc = include_directories('.')

cc = meson.get_compiler('c')

config_h = configuration_data()

# package defines
set_defines = [
  # package
  ['PACKAGE_NAME', meson.project_name()],
  ['PACKAGE_STRING', '@0@ @1@'.format(glade_name, glade_version)],
  ['PACKAGE_VERSION', glade_version],
  ['VERSION', glade_version],
  # i18n
  ['GETTEXT_PACKAGE', glade_name],
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

config_h.set('GLADE_MAJOR_VERSION', glade_major_version)
config_h.set('GLADE_MINOR_VERSION', glade_minor_version)
config_h.set('GLADE_MICRO_VERSION', glade_micro_version)
config_h.set('GDK_VERSION_MIN_REQUIRED', '(G_ENCODE_VERSION (3, 0))')
config_h.set('ENABLE_NLS', true)

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']

if get_option('buildtype').contains('debug')
  common_flags += cc.get_supported_arguments('-Wnested-externs')
endif

add_project_arguments(common_flags, language: 'c')

# Check required libraries
glib_dep = dependency('glib-2.0', version: '>= 2.53.2')
gmodule_dep = dependency('gmodule-2.0')
gmodule_export_dep = dependency('gmodule-export-2.0')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.22.0')
libxml_dep = dependency('libxml-2.0', version: '>= 2.4.0')

m_dep = cc.find_library('m')

# Check for the unix print widgets in gtk+
have_gtk_unix_print = cc.has_function('gtk_print_unix_dialog_new', dependencies: gtk_dep)

# Check for _gtk_widget_class_template_unset_only_for_glade() in gtk+
config_h.set10('HAVE_GTK_TEMPLATE_UNSET', cc.has_function('_gtk_widget_class_template_unset_only_for_glade', dependencies: gtk_dep))

# va_copy.
va_copy_src = '''
    #include <stdarg.h>
    int main() {
        va_list ap1, ap2;
        @0@(ap1, ap2);
    };
'''

if not cc.links(va_copy_src.format('va_copy', name: 'how to copy va_list'))
  if cc.links(va_copy_src.format('__va_copy'))
    config_h.set('va_copy', '__va_copy')
  else
    config_h.set('va_copy(dest,src)', 'memcpy(&dest,&src,sizeof(va_list))')
  endif
endif

# Python for optional python dev libs
pygobject_version = '3.8.0'
pygobject_dep = dependency('pygobject-3.0', version: '>= ' + pygobject_version, required: get_option('python'))
have_python = pygobject_dep.found()
if have_python
  python_dep = import('python').find_installation().dependency()

  version_array = pygobject_version.split('.')
  config_h.set('PYGOBJECT_REQUIRED_MAJOR', version_array[0].to_int())
  config_h.set('PYGOBJECT_REQUIRED_MINOR', version_array[1].to_int())
  config_h.set('PYGOBJECT_REQUIRED_MICRO', version_array[2].to_int())
endif

# WebKit2GTK support
webkit2gtk_dep = dependency('webkit2gtk-4.0', version: '>= 2.12.0', required: get_option('webkit2gtk'))
have_webkit2gtk = webkit2gtk_dep.found()

# Check for GDK Quartz and MacOSX integration package
enable_mac_integration = dependency('gdk-3.0').get_pkgconfig_variable('targets').contains('quartz')
if enable_mac_integration
  gtk_mac_integration_gtk3_dep = dependency('gtk-mac-integration-gtk3')

  if get_option('mac_bundle')
    gtk_mac_bundle_flags = '-DMAC_BUNDLE'
  endif
endif

# Check for osx
config_h.set('PLATFORM_OSX', glade_system == 'darwin')

if glade_system == 'windows'
  rc_conf = configuration_data()
  rc_conf.set('GLADE_MAJOR_VERSION', glade_major_version)
  rc_conf.set('GLADE_MINOR_VERSION', glade_minor_version)
  rc_conf.set('GLADE_MICRO_VERSION', glade_micro_version)
  rc_conf.set('GLADE_CURRENT_MINUS_AGE', gladeui_current_minus_age)
endif

subdir('po')
subdir('data')
subdir('gladeui')
subdir('src')
subdir('plugins')
subdir('tests')
subdir('help')

enable_gtk_doc = get_option('gtk_doc')
if enable_gtk_doc
  subdir('doc')
endif

enable_man = get_option('man')
if enable_man
  subdir('man')
endif

configure_file(
  output: 'config.h',
  configuration: config_h,
)

meson.add_install_script(
  'meson_post_install.py',
  glade_datadir,
)

output = '\nConfiguration:\n\n'
output += '\tCompiler:                ' + cc.get_id() + '\n'
output += '\tSource code location:    ' + source_root + '\n'
output += '\tDebug Enabled:           ' + glade_debug.to_string() + '\n'
output += '\tGTK+ UNIX Print Widgets: ' + have_gtk_unix_print.to_string() + '\n'
output += '\tPYTHON Widgets support:  ' + have_python.to_string() + '\n'
output += '\tGladeui Catalog:         ' + enable_gladeui.to_string() + '\n'
output += '\tWebKit2GTK+ Catalog:     ' + have_webkit2gtk.to_string() + '\n'
output += '\tIntrospection Data:      ' + enable_introspection.to_string() + '\n\n'
output += '\tBuild Reference Manual:  ' + enable_gtk_doc.to_string() + '\n'
output += '\tBuild Manual Pages:      ' + enable_man.to_string()
message(output)
