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
