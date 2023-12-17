project('parlatype', 'c',
        version: '4.0',
        meson_version: '>= 0.56.0',
        license: 'GPL3+',
)

app_id = 'org.parlatype.Parlatype'
version = meson.project_version()

with_pocketsphinx = get_option('pocketsphinx')
with_deepspeech   = get_option('deepspeech')
with_asr = with_pocketsphinx or with_deepspeech

prefix     = get_option('prefix')
bindir     = join_paths(prefix, get_option('bindir'))
libdir     = join_paths(prefix, get_option('libdir'))
datadir    = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
localedir  = join_paths(prefix, get_option('localedir'))
add_global_arguments('-Wno-deprecated-declarations',
                     '-DLOCALEDIR="@0@"'.format(localedir),
                     language : 'c')
compiler = meson.get_compiler('c')

gettext_package = 'parlatype'

conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://www.parlatype.org')
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
gtk        = dependency('gtk4',         version: '>= 4.10')
glib       = dependency('glib-2.0',     version: '>= 2.70')
libadwaita = dependency('libadwaita-1', version: '>= 1.4')

subdir('libparlatype')
subdir('data')
subdir('docs')
subdir('help')
subdir('po')
subdir('src')

# tools are not shipped in tarball
fs = import('fs')
if fs.is_dir('tools')
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
  'gtk-doc'     : gtk_doc,
  'gir'         : gir,
  'pocketsphinx': with_pocketsphinx,
  'deepspeech': with_deepspeech,
  }, section: 'Configuration',
)
