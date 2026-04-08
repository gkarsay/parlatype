plugindir = join_paths(get_option('libdir'), 'parlatype/plugins')
plugindatadir = join_paths(get_option('datadir'), 'parlatype/plugins')

plugins_sources = []
plugins_include_directories = []
plugins_generated_sources = []
plugins_link_with = []

plugins_deps = [
  libparlatype_static_dep,
]

subdir('dummy')
subdir('mpris')
subdir('pocketsphinx')
subdir('whisper')

plugins = static_library('plugins', plugins_sources,
           dependencies: plugins_deps,
#                 c_args: release_args,
    include_directories: plugins_include_directories,
              link_with: plugins_link_with,
#  gnu_symbol_visibility: 'hidden',
)