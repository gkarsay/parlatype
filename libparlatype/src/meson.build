gresources = gnome.compile_resources(
	'pt-lib-resources',
	'resources/libparlatype.gresource.xml',
	source_dir: 'resources',
	c_name: '_pt',
)

marshalers = gnome.genmarshal(
	'pt-marshalers',
	sources: 'ptmarshalers.list',
	prefix: '_pt_cclosure_marshal',
	valist_marshallers: true,
)

marshal_h = marshalers[1]

sources = [
	gresources,
	marshalers,
	'gst/gst-helpers.c',
	'gst/gst-helpers.h',
	'gst/gstptaudiobin.c',
	'gst/gstptaudiobin.h',
	'gst/gstptaudioasrbin.c',
	'gst/gstptaudioasrbin.h',
	'gst/gstptaudioplaybin.c',
	'gst/gstptaudioplaybin.h',
	'pt-config.c',
	'pt-i18n.c',
	'pt-i18n.h',
	'pt-player.c',
	'pt-position-manager.c',
	'pt-position-manager.h',
	'pt-waveloader.c',
	'pt-waveviewer.c',
	'pt-waveviewer-cursor.c',
	'pt-waveviewer-cursor.h',
	'pt-waveviewer-focus.c',
	'pt-waveviewer-focus.h',
	'pt-waveviewer-ruler.c',
	'pt-waveviewer-ruler.h',
	'pt-waveviewer-scrollbox.c',
	'pt-waveviewer-scrollbox.h',
	'pt-waveviewer-selection.c',
	'pt-waveviewer-selection.h',
	'pt-waveviewer-waveform.c',
	'pt-waveviewer-waveform.h',
]

deps = [config_h,
	gnome_desktop,
	gtk,
	gstreamer,
	gstreamer_app,
	gstreamer_audio,
	math,
]

if with_pocketsphinx
	sources += [
	'gst/gstparlasphinx.c',
	'gst/gstparlasphinx.h',
	]

	sphinxbase = dependency('sphinxbase')
	pocketsphinx = dependency('pocketsphinx')
	deps += [sphinxbase, pocketsphinx]
endif

if with_deepspeech
	sources += [
	'gst/gstptdeepspeech.c',
	'gst/gstptdeepspeech.h',
	'gst/vad_private.c',
	'gst/vad_private.h',
	]

	deepspeech = compiler.find_library('deepspeech')
	deps += [deepspeech]
endif

headers = [
	'pt-config.h',
	'pt-player.h',
	'pt-waveloader.h',
	'pt-waveviewer.h',
]

install_headers(headers, subdir: 'parlatype')

mapfile = 'libparlatype.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
libparlatype = shared_library(
	'parlatype',
	sources: sources,
	version: libparlatype_version,
	dependencies: deps,
	include_directories: conf_inc,
	link_args: vflag,
	link_depends: mapfile,
	install: true,
	c_args: [
		'-DPARLATYPE_COMPILATION',
		'-DG_LOG_DOMAIN="libparlatype"',
	],
)

libparlatype_dep = declare_dependency(
	link_with: libparlatype,
	include_directories: conf_inc,
	dependencies: [ gtk, ],
)

pkg = import('pkgconfig')

pkg.generate(
	libparlatype,
	filebase: meson.project_name(),
	name: meson.project_name(),
	description: 'libparlatype, core support for parlatype',
	version: meson.project_version(),
	subdirs: meson.project_name(),
	requires: [
		'gtk+-3.0',
	],
	requires_private: [
		'gstreamer-1.0',
	],
)

gir = get_option('gir')

if gir
	gir_dep = dependency('gobject-introspection-1.0')
	introspection_sources = [
		headers,
		'pt-config.c',
		'pt-player.c',
		'pt-waveloader.c',
		'pt-waveviewer.c',
	]

	gnome.generate_gir(libparlatype,
		sources: introspection_sources,
		nsversion: libparlatype_api_version,
		namespace: 'Parlatype',
		symbol_prefix: 'pt_',
		identifier_prefix: 'Pt',
		includes: ['GObject-2.0', 'Gtk-3.0'],
		install: true
	)
endif