install_data(
	'Parlatype.py',
	install_dir: get_option('libreoffice-dir')
)

lo_metainfo = app_id + '.libreoffice_helpers.metainfo.xml'
i18n.merge_file(
	input: lo_metainfo + '.in',
	output: lo_metainfo,
	type: 'xml',
	po_dir: '../po',
	install: true,
	install_dir: join_paths(datadir, 'metainfo')
)

# searched for appstream_util in ../data/
if appstream_util.found()
	test('Validating ' + lo_metainfo,
		appstream_util,
		args: ['validate-relax', '--nonet', lo_metainfo],
		workdir: meson.current_build_dir()
	)
endif
