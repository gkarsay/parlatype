tests = [
  { 'name': 'config'                             },
  { 'name': 'player'                             },
  { 'name': 'waveloader'                         },
  { 'name': 'waveviewer'                         },
  { 'name': 'gst',              'internal': true },
  { 'name': 'waveloader-static','internal': true },
]

test_data = [
  'data/test1.ogg',
]

extra_sources = [
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

foreach t: tests
  test_name = t.get('name')
  if t.get('internal', false)
    dep = libparlatype_static_dep
  else
    dep = libparlatype_dep
  endif

  exe = executable(
    test_name, '@0@.c'.format(test_name), extra_sources,
    install: false,
    dependencies: dep,
    c_args: '-DPARLATYPE_COMPILATION',
  )

  test(
    test_name, exe,
    args: [ '--tap', '-k' ],
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'LANGUAGE=C',
    ],
    depends: asr_config_data,
  )
endforeach
