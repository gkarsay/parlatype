if get_option('plugin_whisper')

whisper = dependency('whisper', version: '>= 1.8.0')
plugins_deps += whisper

plugins_sources += files([
  'whisper-plugin.c',
  'ptp-whisper.c',
])

plugin_whisper_resources = gnome.compile_resources(
  'whisper-resources',
  'whisper.gresource.xml',
  c_name: 'whisper',
)

plugins_sources += plugin_whisper_resources

endif