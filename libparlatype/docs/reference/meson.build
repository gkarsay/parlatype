version_conf = configuration_data()
version_conf.set('PACKAGE_VERSION', meson.project_version())
configure_file(
	input: 'version.xml.in',
	output: 'version.xml',
	configuration: version_conf,
)

gnome.gtkdoc(
	'parlatype',
	main_xml: 'parlatype-docs.xml',
	src_dir: [
		join_paths(meson.source_root(), 'libparlatype', 'src'),
		join_paths(meson.build_root(), 'libparlatype', 'src'),
	],
	gobject_typesfile: join_paths(meson.current_source_dir(), 'libparlatype.types'),
	dependencies: libparlatype_dep,
	scan_args: [
		'--source-dir=' + join_paths(meson.source_root(), 'libparlatype', 'src'),
		'--rebuild-types',
	],
	mkdb_args: [
		'--xml-mode',
		'--output-format=xml',
	],
	content_files: [
		'gallery.xml',
		'introduction.xml',
		'using_library.xml',
		'python_example.xml',
		'parlatype-example.py',
	],
	ignore_headers: [
		'pt-waveviewer-cursor.h',
		'pt-waveviewer-focus.h',
		'pt-waveviewer-selection.h',
		'pt-waveviewer-ruler.h',
		'pt-waveviewer-waveform.h',
	],
	html_assets: [
		'images/pt-progress-dialog.png',
		'images/pt-waveviewer.png',
	],
	install: true,
)
