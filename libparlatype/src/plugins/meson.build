#plugindir = join_paths(get_option('libdir'), 'gnome-builder/plugins')
#plugindatadir = join_paths(get_option('datadir'), 'gnome-builder/plugins')

plugins_sources = []
#plugins_include_directories = []
plugins_generated_sources = []
plugins_link_with = []

plugins_deps = []
plugins_deps += libparlatype_deps

with_pocketsphinx = get_option('plugin_pocketsphinx')

with_asr = with_pocketsphinx

if with_pocketsphinx
  pocketsphinx = dependency('pocketsphinx', version: '>= 5.0')
  libparlatype_deps += [pocketsphinx]
endif

subdir('dummy')
subdir('pocketsphinx')
subdir('whisper')

plugins = static_library('plugins', plugins_sources,
           dependencies: plugins_deps,
#                 c_args: release_args,
    include_directories: libconf_inc, #plugins_include_directories,
              link_with: plugins_link_with,
#  gnu_symbol_visibility: 'hidden',
)