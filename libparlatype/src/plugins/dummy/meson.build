if get_option('plugin_dummy')

plugins_sources += files([
  'dummy-plugin.c',
  'dummy-greeter.c',
])

plugin_dummy_resources = gnome.compile_resources(
  'dummy-greeter-resources',
  'dummy.gresource.xml',
  c_name: 'dummy_greeter',
)

plugins_sources += plugin_dummy_resources

endif