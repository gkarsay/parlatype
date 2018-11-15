gresources = gnome.compile_resources(
	'pt-lib-resources',
	'resources/libparlatype.gresource.xml',
	source_dir: 'resources',
	c_name: '_pt',
)

marshalers = [
	'pt-marshalers.c',
	'pt-marshalers.h',
]

marshal_h = marshalers[1]

sources = [
	gresources,
	marshalers,
	'pt-asr-settings.c',
	'pt-player.c',
	'pt-progress-dialog.c',
	'pt-wavedata.c',
	'pt-waveloader.c',
	'pt-waveviewer.c',
	'pt-waveviewer-cursor.c',
	'pt-waveviewer-cursor.h',
	'pt-waveviewer-focus.c',
	'pt-waveviewer-focus.h',
	'pt-waveviewer-ruler.c',
	'pt-waveviewer-ruler.h',
	'pt-waveviewer-selection.c',
	'pt-waveviewer-selection.h',
	'pt-waveviewer-waveform.c',
	'pt-waveviewer-waveform.h',
]

deps = [gtk, gmodule, gstreamer, gstreamer_audio, math]

headers = [
	'pt-asr-settings.h',
	'pt-player.h',
	'pt-progress-dialog.h',
	'pt-wavedata.h',
	'pt-waveloader.h',
	'pt-waveviewer.h',
]

install_headers(headers, subdir: 'parlatype')

mapfile = 'libparlatype.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
libparlatype = shared_library(
	'parlatype',
	sources: sources,
	soversion: libparlatype_so_version,
	dependencies: deps,
	include_directories: conf_inc,
	link_args: vflag,
	link_depends: mapfile,
	install: true,
)

libparlatype_dep = declare_dependency(
	link_with: libparlatype,
	include_directories: conf_inc,
	dependencies: [ gtk, ],
)

gir = get_option('gir')

if gir
	gir_dep = dependency('gobject-introspection-1.0')
	introspection_sources = [
		headers,
		'pt-asr-settings.c',
		'pt-player.c',
		'pt-progress-dialog.c',
		'pt-wavedata.c',
		'pt-waveloader.c',
		'pt-waveviewer.c',
	]

	gnome.generate_gir(libparlatype,
		sources: introspection_sources,
		nsversion: '1.0',
		namespace: 'Parlatype',
		symbol_prefix: 'pt_',
		identifier_prefix: 'Pt',
		includes: ['GObject-2.0', 'Gtk-3.0'],
		install: true
	)
endif
