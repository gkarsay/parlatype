if get_option('docs')

gidocgen = find_program('gi-docgen')

docs_dir = join_paths(get_option('datadir'), 'doc')

toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

libparlatype_toml = configure_file(
    input: 'libparlatype.toml.in',
    output: 'libparlatype.toml',
    configuration: toml_conf,
    install: true,
    install_dir: docs_dir / 'libparlatype',
)

custom_target('libparlatype-doc',
    input: [ libparlatype_toml, libparlatype_gir[0] ],
    output: 'libparlatype',
    command: [
      gidocgen,
      'generate',
      '--quiet',
      '--fatal-warnings',
      '--add-include-path=@0@'.format(meson.current_build_dir() / '../libparlatype'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
)

test('Check docs',
     gidocgen,
     args: [
      'check',
      libparlatype_gir[0],
      ],
     workdir: meson.current_build_dir()
)

endif