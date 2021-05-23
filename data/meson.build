subdir('icons')

install_subdir(
	'asr',
	install_dir: join_paths(datadir, meson.project_name())
)

desktop_file = app_id + '.desktop'
i18n.merge_file(
	input: desktop_file + '.in',
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

appdata = app_id + '.appdata.xml'
i18n.merge_file(
	input: appdata + '.in',
	output: appdata,
	type: 'xml',
	po_dir: '../po',
	install: true,
	install_dir: join_paths(datadir, 'metainfo')
)


appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
	test('Validating ' + appdata,
		appstream_util,
		args: ['validate-relax', '--nonet', appdata],
		workdir: meson.current_build_dir()
	)
endif

install_data(
	app_id + '.gschema.xml',
	install_dir: join_paths(datadir, 'glib-2.0', 'schemas'))

service_conf = configuration_data()
service_conf.set ('bindir', bindir)
configure_file(
	input: app_id + '.service.in',
	output: app_id + '.service',
	configuration: service_conf,
	install_dir: join_paths(datadir, 'dbus-1', 'services')
)

meson.add_install_script('meson_post_install.py')