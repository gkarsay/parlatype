if get_option('plugin_test_buttons')

plugins_sources += files([
  'test-buttons-plugin.c',
  'ptp-test-buttons.c',
])

plugin_test_buttons_resources = gnome.compile_resources(
  'test-buttons-resources',
  'test-buttons.gresource.xml',
  c_name: 'test_buttons',
)

plugins_sources += plugin_test_buttons_resources

install_data(
  'test-buttons.gschema.xml',
  rename: app_id + '.test-buttons.gschema.xml',
  install_dir: schema_dir
)

endif