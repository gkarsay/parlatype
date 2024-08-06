screenshots = executable('pt-screenshots',
  [ gresources, 'screenshots.c' ],
  link_with: parlatype_applib_static,
  include_directories : conf_inc,
  install: false,
  dependencies: parlatype_deps,
  c_args: [
    '-DPT_SOURCE_DIR="@0@"'.format(meson.project_source_root()),
    '-DPT_BUILD_DIR="@0@"'.format(meson.project_build_root()),
  ],
)

run_target('generate-screenshots',
  command: '../maint/generate-screenshots')