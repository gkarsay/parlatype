project('parlatype', 'c',
	version: '1.6',
	#meson_version: '>= 0.47.2', # oldest working tested version
	license: 'GPL3+',
)

app_id = 'com.github.gkarsay.parlatype'

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://gkarsay.github.io/parlatype/')
configure_file(output: 'config.h', configuration: conf)
conf_inc = include_directories('.', 'libparlatype/src')

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
localedir = join_paths(prefix, get_option('localedir'))
add_global_arguments('-DLOCALEDIR="@0@"'.format(localedir), language : 'c')

gnome = import('gnome')
i18n = import('i18n')

gettext_package = 'parlatype'

# Checks for libraries.
gtk = dependency('gtk+-3.0', version: '>= 3.22')
gmodule = dependency('gmodule-2.0')
atspi = dependency('atspi-2')

# GLib versioned dependency is for the build system,
# see https://bugzilla.gnome.org/show_bug.cgi?id=774368
glib = dependency('glib-2.0', version: '>= 2.50.2')

libreoffice = get_option('libreoffice')

subdir('libparlatype')
subdir('data')
subdir('docs')
subdir('help')
subdir('po')
subdir('src')

if libreoffice
	subdir('libreoffice')
endif
