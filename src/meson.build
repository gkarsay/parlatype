gresources = gnome.compile_resources(
  'pt-win-resources',
  'resources/parlatype.gresource.xml',
  source_dir: 'resources',
  c_name: '_pt',
)

parlatype_sources = [
  'editor-theme-selector.c',
  'pt-app.c',
  'pt-asr-dialog.c',
  'pt-config-list.c',
  'pt-config-row.c',
  'pt-controller.c',
  'pt-dbus-service.c',
  'pt-goto-dialog.c',
  'pt-mpris.c',
  'pt-preferences.c',
  'pt-prefs-info-row.c',
  'pt-prefs-install-row.c',
  'pt-window.c',
  'pt-window-dnd.c',
]

parlatype_deps = [
  config_h,
  gtk,
  libadwaita,
]

# Static library for tools
if build_tools
  parlatype_applib_static = static_library(
    'parlatype-applib-static',
    sources: parlatype_sources,
    include_directories: conf_inc,
    link_with: libparlatype,
    dependencies: parlatype_deps,
    c_args: [
      '-DG_LOG_DOMAIN="libparlatype"',
    ],
  )
endif

executable('parlatype',
           [ gresources, parlatype_sources, 'main.c' ],
           link_with: libparlatype,
           include_directories : conf_inc,
           install: true,
           dependencies: parlatype_deps,
           c_args: ['-DG_LOG_DOMAIN="parlatype"' ],
)