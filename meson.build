project('parlatype', 'c',
        version: '4.3',
        meson_version: '>= 0.60.0',
        license: 'GPL3+',
        default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
)

app_id = 'xyz.parlatype.Parlatype'
version = meson.project_version()
version_split = version.split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
libparlatype_api_version = '@0@'.format(MAJOR_VERSION)

prefix     = get_option('prefix')
bindir     = join_paths(prefix, get_option('bindir'))
libdir     = join_paths(prefix, get_option('libdir'))
datadir    = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
schema_dir = join_paths(datadir, 'glib-2.0', 'schemas')
localedir  = join_paths(prefix, get_option('localedir'))

compiler = meson.get_compiler('c')

add_project_arguments('-DLOCALEDIR="@0@"'.format(localedir),
                      '-Wno-cast-function-type',
                      '-Wno-missing-field-initializers',
                      '-Wno-unused-parameter',
                      '-Wswitch-default',
                       language: 'c')

gettext_package = 'parlatype'

gnome   = import('gnome')
i18n    = import('i18n')

# Checks for libraries.
gtk             = dependency('gtk4', version: '>= 4.14')
glib            = dependency('glib-2.0', version: '>= 2.76')
libadwaita      = dependency('libadwaita-1', version: '>= 1.5')
libxml          = dependency('libxml-2.0')
libpeas         = dependency('libpeas-2', version: '>= 1.99.0')
girepository    = dependency('girepository-2.0', version: '>= 2.80')
gstreamer       = dependency('gstreamer-1.0', version : '>= 1.6.3')
gstreamer_app   = dependency('gstreamer-app-1.0')
gstreamer_audio = dependency('gstreamer-audio-1.0')
iso_codes       = dependency('iso-codes')
iso_codes_prefix = iso_codes.get_variable(pkgconfig: 'prefix')
math = compiler.find_library('m', required: false)

conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://www.parlatype.xyz')
conf.set_quoted('APP_ID', app_id)
conf.set_quoted('ASR_DIR', join_paths(datadir, meson.project_name(), 'asr'))
conf.set('HAVE_ASR', true)
conf.set_quoted('GETTEXT_LIB', 'libparlatype7')
conf.set_quoted('ISO_CODES_PREFIX', iso_codes_prefix)
conf_input = configure_file(output: 'config.h.in', configuration: conf)

config_h = declare_dependency(
  sources: vcs_tag(
    command: [ 'git', 'describe', '--tags' ],
    input: conf_input,
    output: 'config.h',
  )
)

# tools are not shipped in tarball, check if directory exists
fs = import('fs')
build_tools = get_option('tools') and fs.is_dir('tools')

subdir('data')
#subdir('help')
subdir('po')
subdir('src')
subdir('docs') # after src, needs gir

if build_tools
  subdir('tools')
endif


summary('prefix', prefix, section: 'Directories')
if (prefix != '/usr')
  summary(
    'Run command',
    'LD_LIBRARY_PATH=@0@ XDG_DATA_DIRS=$XDG_DATA_DIRS:@1@ @2@/parlatype'.format(libdir, datadir, bindir),
    section: 'Directories')
endif

summary({
  'docs'               : get_option('docs'),
  'gir'                : gir,
  'tools'              : build_tools,
  }, section: 'Configuration',
)
