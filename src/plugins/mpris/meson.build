if get_option('plugin_mpris')

plugins_sources += files([
  'mpris-plugin.c',
  'ptp-mpris.c',
])

plugin_mpris_resources = gnome.compile_resources(
  'mpris-resources',
  'mpris.gresource.xml',
  c_name: 'mpris',
)

plugins_sources += plugin_mpris_resources

endif