subdir('icons')

desktop_file = app_id + '.desktop'
i18n.merge_file(
  input: 'parlatype.desktop.in',
  output: desktop_file,
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(datadir, 'applications')
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('Validating ' + desktop_file,
       desktop_file_validate,
       args: [desktop_file],
       workdir: meson.current_build_dir()
  )
endif

metainfo = app_id + '.metainfo.xml'
i18n.merge_file(
  input: 'parlatype.metainfo.xml.in',
  output: metainfo,
  type: 'xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(datadir, 'metainfo')
)


appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validating ' + metainfo,
       appstream_util,
       args: ['validate-relax', '--nonet', metainfo],
       workdir: meson.current_build_dir()
  )
endif

schema_files = [configure_file(
   build_subdir: join_paths('glib-2.0','schemas'),
          input: 'parlatype.gschema.xml',
         output: app_id + '.gschema.xml',
           copy: true,
        install: true,
    install_dir: schema_dir,
)]

if (get_option('gir'))
  gschemas_compiled = custom_target('gschemas.compiled',
              output: 'gschemas.compiled',
             command: ['glib-compile-schemas', join_paths(meson.current_build_dir(),'glib-2.0','schemas')],
    build_by_default: true,
             install: false,
  )
endif

service_conf = configuration_data()
service_conf.set ('app_id', app_id)
service_conf.set ('bindir', bindir)
configure_file(
  input: 'parlatype.service.in',
  output: app_id + '.service',
  configuration: service_conf,
  install_dir: join_paths(datadir, 'dbus-1', 'services')
)

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
)
