project('glade', 'c', version: '3.22.0')

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')

glib_dep = dependency('glib-2.0', version: '>=2.53.2')
gtk_dep = dependency('gtk+-3.0', version: '>=3.22.0')
gmodule_dep = dependency('gmodule-2.0')
libxml_dep = dependency('libxml-2.0', version: '>=2.4.0')
gmodule_export_dep = dependency('gmodule-export-2.0')
m_dep = cc.find_library('m', required: false)

if cc.has_function ('gtk_print_unix_dialog_new', dependencies: gtk_dep)
  add_project_arguments('-DHAVE_GTK_UNIX_PRINT', language: 'c')
endif
if cc.has_function ('_gtk_widget_class_template_unset_only_for_glade', dependencies: gtk_dep)
  add_project_arguments('-DHAVE_GTK_TEMPLATE_UNSET', language: 'c')
endif

webkit2gtk_dep = dependency('webkit2gtk-4.0', version: '>=2.12.0', required: get_option('webkit2gtk'))
if webkit2gtk_dep.found()
  add_project_arguments('-DBUILD_WEBKIT2GTK', language: 'c')
endif

add_project_arguments('-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()), language: 'c')
add_project_arguments('-DPACKAGE_NAME="@0@"'.format(meson.project_name()), language: 'c')
add_project_arguments('-DPACKAGE_STRING="@0@"'.format(meson.project_name()), language: 'c')
add_project_arguments('-DPACKAGE_VERSION="@0@"'.format(meson.project_version()), language: 'c')
add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'c')
version_parts = meson.project_version().split('.')
add_project_arguments('-DGLADE_MAJOR_VERSION=@0@'.format(version_parts[0]), language: 'c')
add_project_arguments('-DGLADE_MINOR_VERSION=@0@'.format(version_parts[1]), language: 'c')
add_project_arguments('-DGLADE_MICRO_VERSION=@0@'.format(version_parts[2]), language: 'c')

root_directory = include_directories('.')

config_header = configure_file(
  output: 'config.h',
  configuration: configuration_data()
)

subdir('gladeui')
subdir('src')
subdir('doc')
subdir('help')
subdir('po')
subdir('data')
