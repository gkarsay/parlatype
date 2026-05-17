if get_option('plugin_fake_downloader')

plugin_name = 'fake_downloader'

plugin_dir = join_paths(pluginsdir, plugin_name)

install_data(
  plugin_name + '.py',
  plugin_name + '.plugin',
  install_dir: plugin_dir
)

install_subdir(
  'asr',
  install_dir: join_paths(datadir, meson.project_name())
)

endif