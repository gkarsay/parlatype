project('parlatype', 'c',
	version: '1.6.2',
	#meson_version: '>= 0.47.2', # oldest working tested version
	license: 'GPL3+',
)

app_id = 'com.github.gkarsay.parlatype'

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

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
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
