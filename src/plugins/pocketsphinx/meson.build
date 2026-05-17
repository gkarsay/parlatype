if get_option('plugin_pocketsphinx')

pocketsphinx = dependency('pocketsphinx', version: '>= 5.0')
plugins_deps += pocketsphinx

plugins_sources += files([
  'pocketsphinx-plugin.c',
  'ptp-pocketsphinx.c',
])

plugin_pocketsphinx_resources = gnome.compile_resources(
  'pocketsphinx-resources',
  'pocketsphinx.gresource.xml',
  c_name: 'pocketsphinx',
)

plugins_sources += plugin_pocketsphinx_resources

install_subdir(
  'asr',
  install_dir: join_paths(datadir, meson.project_name())
)

endif