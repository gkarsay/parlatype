tests = [
  'config',
  'gst',
  'player',
  'waveloader-static',
  'waveloader',
  'waveviewer',
]

test_data = [
  'data/test1.ogg',
]

extra_sources = [
  '../src/contrib/gnome-languages.c',
  '../src/contrib/gnome-languages.h',
  '../src/gst/gst-helpers.c',
  '../src/gst/gst-helpers.h',
  '../src/gst/gstptaudiobin.c',
  '../src/gst/gstptaudiobin.h',
  '../src/gst/gstptaudioasrbin.c',
  '../src/gst/gstptaudioasrbin.h',
  '../src/gst/gstptaudioplaybin.c',
  '../src/gst/gstptaudioplaybin.h',
  '../src/pt-i18n.c',
  '../src/pt-i18n.h',
  'mock-plugin.c',
  'mock-plugin.h',
]

gen_prog = find_program('data/generate_config_data', required: true)

config_input = join_paths(meson.current_source_dir(), 'data', 'config-test.asr')

asr_config_data = custom_target(
  'asr-config-data',
  command: [
    gen_prog,
    config_input,
    meson.current_build_dir()
  ],
  input: config_input,
  output: 'config-test.asr', # actually many more
)

contrib_sources = [
  '../src/contrib/gnome-languages.c',
  '../src/contrib/gnome-languages.h',
]

contrib_exe = executable(
  'contrib', 'contrib.c', contrib_sources,
  install: false,
  dependencies: [
    libparlatype_dep,
  ],
)

locales = [ 'C', 'de' ]
foreach l: locales
  test(
    'contrib', contrib_exe,
    args: [ '--tap', '-k' ],
    env: [
      'LANGUAGE=@0@'.format(l),
    ],
  )
endforeach


foreach t: tests
  exe = executable(
    t, '@0@.c'.format(t), extra_sources,
    install: false,
    dependencies: [
      config_h,
      libparlatype_dep,
      gstreamer,
      gstreamer_app,
      gstreamer_audio,
    ],
    c_args: '-DPARLATYPE_COMPILATION',
  )

  test(
    t, exe,
    args: [ '--tap', '-k' ],
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'LANGUAGE=C',
    ],
    depends: asr_config_data,
  )
endforeach
