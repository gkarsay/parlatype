gresources = gnome.compile_resources(
  'pt-win-resources',
  'resources/parlatype.gresource.xml',
  source_dir: 'resources',
  c_name: '_pt',
)

src = [
  gresources,
  'main.c',
  'pt-app.c',
  'pt-app.h',
  'pt-asr-dialog.c',
  'pt-asr-dialog.h',
  'pt-config-list.c',
  'pt-config-list.h',
  'pt-config-row.c',
  'pt-config-row.h',
  'pt-controller.c',
  'pt-controller.h',
  'pt-dbus-service.c',
  'pt-dbus-service.h',
  'pt-goto-dialog.c',
  'pt-goto-dialog.h',
  'pt-mpris.c',
  'pt-mpris.h',
  'mpris-spec.h',
  'pt-preferences.c',
  'pt-preferences.h',
  'pt-prefs-asr.c',
  'pt-prefs-asr.h',
  'pt-prefs-info-row.c',
  'pt-prefs-info-row.h',
  'pt-window.c',
  'pt-window.h',
  'pt-window-dnd.c',
  'pt-window-dnd.h',
  'pt-window-private.h',
]

parlatype_deps = [
  config_h,
  gtk,
  libadwaita,
]

executable('parlatype',
           src,
           link_with: libparlatype,
           include_directories : conf_inc,
           install: true,
           dependencies: parlatype_deps,
           c_args: ['-DG_LOG_DOMAIN="parlatype"' ],
)
