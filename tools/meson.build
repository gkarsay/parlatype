math = compiler.find_library('m', required: false)

screenshots = executable('pt-screenshots', 'screenshots.c',
              install: false,
           link_whole: parlatype_static,
         dependencies: parlatype_deps,
               c_args: [
                         '-DPT_SOURCE_DIR="@0@"'.format(meson.project_source_root()),
                         '-DPT_BUILD_DIR="@0@"'.format(meson.project_build_root()),
                       ],
)

run_target('generate-screenshots',
  command: '../maint/generate-screenshots')