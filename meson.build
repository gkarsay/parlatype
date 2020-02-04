project('parlatype', 'c',
	version: '1.6.2',
	#meson_version: '>= 0.47.2', # oldest working tested version
	license: 'GPL3+',
)

app_id = 'com.github.gkarsay.parlatype'

version       = meson.project_version()
version_array = version.split('.')
major_version = version_array[0].to_int()
minor_version = version_array[1].to_int()
micro_version = version_array[2].to_int()

libreoffice = get_option('libreoffice')
asr =         get_option('asr')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://gkarsay.github.io/parlatype/')
conf.set('HAVE_ASR', asr)
configure_file(output: 'config.h', configuration: conf)
conf_inc = include_directories('.', 'libparlatype/src')

prefix     = get_option('prefix')
bindir     = join_paths(prefix, get_option('bindir'))
libdir     = join_paths(prefix, get_option('libdir'))
datadir    = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
localedir  = join_paths(prefix, get_option('localedir'))
add_global_arguments('-DLOCALEDIR="@0@"'.format(localedir), language : 'c')
compiler = meson.get_compiler('c')

gnome   = import('gnome')
i18n    = import('i18n')
windows = import('windows')

gettext_package = 'parlatype'

# Checks for libraries.
gtk = dependency('gtk+-3.0', version: '>= 3.22')
glib = dependency('glib-2.0', version: '>= 2.50.2')

if host_machine.system() == 'windows'
  rc_conf = configuration_data()
  rc_conf.set('MAJOR_VERSION', major_version)
  rc_conf.set('MINOR_VERSION', minor_version)
  rc_conf.set('MICRO_VERSION', micro_version)
  rc_conf.set('VERSION', version)
  rc_conf.set('SOURCE_DIR', meson.current_source_dir())
endif

subdir('libparlatype')
subdir('data')
subdir('docs')
subdir('help')
subdir('po')
subdir('src')

if libreoffice
	subdir('libreoffice')
endif

status = [
  '', '',
  'Parlatype @0@'.format(meson.project_version()),
  '',
  'Prefix ................... : @0@'.format(prefix),
  'With ASR ................. : @0@'.format(asr),
  'With LibreOffice helpers . : @0@'.format(libreoffice)
]

if libreoffice
	status += [
  'Script folder ............ : @0@'.format(get_option('libreoffice-dir'))
	]
endif

status += [
  '',
  'With GObject Introspection : @0@'.format(gir),
  'With API reference ....... : @0@'.format(gtk_doc),
  'With Glade catalog ....... : @0@'.format(glade),
  ''
]

if (prefix != '/usr')
	status += ['Command to run parlatype:']
	if (asr)
		status += ['LD_LIBRARY_PATH=@0@ GST_PLUGIN_PATH=@0@/gstreamer-1.0 XDG_DATA_DIRS=$XDG_DATA_DIRS:@1@ @2@/parlatype'.format(libdir, datadir, bindir)]
	else
		status += ['LD_LIBRARY_PATH=@0@ XDG_DATA_DIRS=$XDG_DATA_DIRS:@1@ @2@/parlatype'.format(libdir, datadir, bindir)]
	endif
	status += ['']
endif

message('\n '.join(status))