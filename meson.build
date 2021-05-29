project('parlatype', 'c',
	version: '2.1',
	#meson_version: '>= 0.47.2', # oldest working tested version
	license: 'GPL3+',
)

app_id = 'org.parlatype.Parlatype'

version       = meson.project_version()
version_array = version.split('.')
major_version = version_array[0].to_int()
minor_version = version_array[1].to_int()
if version_array.length() == 2
	micro_version = 0
else
	micro_version = version_array[2].to_int()
endif

with_pocketsphinx = get_option('pocketsphinx')
with_deepspeech   = get_option('deepspeech')

prefix     = get_option('prefix')
bindir     = join_paths(prefix, get_option('bindir'))
libdir     = join_paths(prefix, get_option('libdir'))
datadir    = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
localedir  = join_paths(prefix, get_option('localedir'))
add_global_arguments('-DLOCALEDIR="@0@"'.format(localedir), language : 'c')
compiler = meson.get_compiler('c')

gettext_package = 'parlatype'
# library message catalog is API versioned for parallel installs
# keep in sync with API version as defined in libparlatype/meson.build
gettext_package_lib = 'libparlatype3'

conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://www.parlatype.org')
conf.set_quoted('APP_ID', app_id)
conf.set_quoted('GETTEXT_LIB', gettext_package_lib)
conf.set_quoted('ASR_DIR', join_paths(datadir, meson.project_name(), 'asr'))
conf.set('HAVE_POCKETSPHINX', with_pocketsphinx)
conf.set('HAVE_DEEPSPEECH', with_deepspeech)
conf_input = configure_file(output: 'config.h.in', configuration: conf)
conf_inc = include_directories('.', 'libparlatype/src')

config_h = declare_dependency(
	sources: vcs_tag(
		command: [ 'git', 'describe', '--tags' ],
		input: conf_input,
		output: 'config.h',
	)
)

gnome   = import('gnome')
i18n    = import('i18n')
windows = import('windows')

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

if host_machine.system() == 'windows'
  meson.add_install_script(
    'meson_post_install_win32.sh',
    app_id
  )
endif

status = [
  '', '',
  'Parlatype @0@'.format(meson.project_version()),
  '',
  'Prefix ................... : @0@'.format(prefix),
  'With Pocketsphinx ........ : @0@'.format(with_pocketsphinx),
  'With Deepspeech .......... : @0@'.format(with_deepspeech),
  'With GObject Introspection : @0@'.format(gir),
  'With API reference ....... : @0@'.format(gtk_doc),
  'With Glade catalog ....... : @0@'.format(glade),
  ''
]

if (prefix != '/usr')
  status += ['Command to run parlatype:']
  status += ['LD_LIBRARY_PATH=@0@ XDG_DATA_DIRS=$XDG_DATA_DIRS:@1@ @2@/parlatype'.format(libdir, datadir, bindir)]
  status += ['']
endif

message('\n '.join(status))