src = [
  gresources,
  'screenshots.c',
  '../src/editor-theme-selector.c',
  '../src/editor-theme-selector-private.h',
  '../src/pt-app.c',
  '../src/pt-app.h',
  '../src/pt-asr-dialog.c',
  '../src/pt-asr-dialog.h',
  '../src/pt-config-list.c',
  '../src/pt-config-list.h',
  '../src/pt-config-row.c',
  '../src/pt-config-row.h',
  '../src/pt-controller.c',
  '../src/pt-controller.h',
  '../src/pt-dbus-service.c',
  '../src/pt-dbus-service.h',
  '../src/pt-goto-dialog.c',
  '../src/pt-goto-dialog.h',
  '../src/pt-mpris.c',
  '../src/pt-mpris.h',
  '../src/pt-preferences.c',
  '../src/pt-preferences.h',
  '../src/pt-prefs-info-row.c',
  '../src/pt-prefs-info-row.h',
  '../src/pt-prefs-install-row.c',
  '../src/pt-prefs-install-row.h',
  '../src/pt-window.c',
  '../src/pt-window.h',
  '../src/pt-window-dnd.c',
  '../src/pt-window-dnd.h',
  '../src/pt-window-private.h',
]

parlatype_deps = [
  config_h,
  gtk,
  libadwaita,
]

screenshots = executable('pt-screenshots',
  src,
  link_with: libparlatype,
  include_directories : conf_inc,
  install: false,
  dependencies: parlatype_deps,
  c_args: [
    '-DPT_SOURCE_DIR="@0@"'.format(meson.project_source_root()),
    '-DPT_BUILD_DIR="@0@"'.format(meson.project_build_root()),
    '-DPARLATYPE_COMPILATION',
  ],
)

run_target('generate-screenshots',
  command: '../maint/generate-screenshots')