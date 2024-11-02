project('parlatype', 'c',
        version: '4.2',
        meson_version: '>= 0.60.0',
        license: 'GPL3+',
        default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
)

app_id = 'xyz.parlatype.Parlatype'
version = meson.project_version()

with_pocketsphinx_legacy = get_option('pocketsphinx-legacy')
with_pocketsphinx = get_option('pocketsphinx')
with_asr = with_pocketsphinx_legacy or with_pocketsphinx
if with_pocketsphinx_legacy and with_pocketsphinx
  error('Options pocketsphinx-legacy and pocketsphinx can’t be used together')
endif

prefix     = get_option('prefix')
bindir     = join_paths(prefix, get_option('bindir'))
libdir     = join_paths(prefix, get_option('libdir'))
datadir    = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
localedir  = join_paths(prefix, get_option('localedir'))

compiler = meson.get_compiler('c')

add_project_arguments('-DLOCALEDIR="@0@"'.format(localedir),
                      '-Wno-cast-function-type',
                      '-Wno-missing-field-initializers',
                      '-Wno-unused-parameter',
                      '-Wswitch-default',
                       language: 'c')

gettext_package = 'parlatype'

conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://www.parlatype.xyz')
conf.set_quoted('APP_ID', app_id)
conf.set_quoted('ASR_DIR', join_paths(datadir, meson.project_name(), 'asr'))
conf.set('HAVE_ASR', with_asr)
conf_input = configure_file(output: 'config.h.in', configuration: conf)
conf_inc = include_directories('.', 'libparlatype/src', 'src')

config_h = declare_dependency(
  sources: vcs_tag(
    command: [ 'git', 'describe', '--tags' ],
    input: conf_input,
    output: 'config.h',
  )
)

gnome   = import('gnome')
i18n    = import('i18n')

# Checks for libraries.
gtk        = dependency('gtk4',         version: '>= 4.14')
glib       = dependency('glib-2.0',     version: '>= 2.76')
libadwaita = dependency('libadwaita-1', version: '>= 1.5')

# tools are not shipped in tarball, check if directory exists
fs = import('fs')
build_tools = fs.is_dir('tools')

subdir('libparlatype')
subdir('data')
subdir('docs')
subdir('help')
subdir('po')
subdir('src')

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
  'gtk-doc'            : gtk_doc,
  'gir'                : gir,
  'pocketsphinx-legacy': with_pocketsphinx_legacy,
  'pocketsphinx'       : with_pocketsphinx,
  }, section: 'Configuration',
)
