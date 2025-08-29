gresources = gnome.compile_resources(
  'pt-win-resources',
  'resources/parlatype.gresource.xml',
  source_dir: 'resources',
  c_name: '_pt',
)

marshalers = gnome.genmarshal(
  'pt-marshalers',
  sources: 'ptmarshalers.list',
  prefix: '_pt_cclosure_marshal',
  valist_marshallers: true,
)

marshal_h = marshalers[1]

parlatype_sources = [
  marshalers,
  'editor-theme-selector.c',
  'pt-app.c',
  'pt-asr-dialog.c',
  'pt-config-list.c',
  'pt-config-row.c',
  'pt-controller.c',
  'pt-dbus-service.c',
  'pt-file-loader.c',
  'pt-file-saver.c',
  'pt-goto-dialog.c',
  'pt-mpris.c',
  'pt-playback-rate-button.c',
  'pt-preferences.c',
  'pt-prefs-info-row.c',
  'pt-prefs-install-row.c',
  'pt-save-changes-dialog.c',
  'pt-text-buffer.c',
  'pt-text-buffer-content.c',
  'pt-text-buffer-input-stream.c',
  'pt-text-buffer-output-stream.c',
  'pt-text-view.c',
  'pt-timestamp-button.c',
  'pt-utils.c',
  'pt-volume-button.c',
  'pt-window.c',
  'pt-window-dnd.c',
]

math = compiler.find_library('m', required: false)

parlatype_deps = [
  config_h,
  gtk,
  libadwaita,
  libxml,
  math,
]

# Static library for tools
if build_tools
  parlatype_applib_static = static_library(
    'parlatype-applib-static',
    sources: parlatype_sources,
    include_directories: conf_inc,
    link_with: libparlatype,
    dependencies: parlatype_deps,
  )
endif

executable('parlatype',
           [ gresources, parlatype_sources, 'main.c' ],
           link_with: libparlatype,
           include_directories : conf_inc,
           install: true,
           dependencies: parlatype_deps,
)