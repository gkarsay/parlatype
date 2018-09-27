pkg = import('pkgconfig')

pkg.generate(
	filebase: meson.project_name(),
	name: meson.project_name(),
	description: 'libparlatype, core support for parlatype',
	version: meson.project_version(),
	requires: [
		'gtk+-3.0',
	],
	requires_private: [
		'gstreamer-1.0',
	],
)

glade = get_option('glade')

if glade
	glade_dep = dependency('gladeui-2.0', version: '>= 3.12.2')
	glade_catalogdir = glade_dep.get_pkgconfig_variable('catalogdir')
	glade_pixmapdir = glade_dep.get_pkgconfig_variable('pixmapdir')
	install_data('parlatype.xml',
		install_dir: glade_catalogdir,
	)
	subdir('icons')
endif
