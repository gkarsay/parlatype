sphinxbase = dependency('sphinxbase')
pocketsphinx = dependency('pocketsphinx')
plugins_install_dir = '@0@/gstreamer-1.0'.format(get_option('libdir'))

gstparlasphinx = library('gstparlasphinx', 'gstparlasphinx.c',
	include_directories: conf_inc,
	dependencies: [sphinxbase, pocketsphinx, gstreamer, gstreamer_audio],
	install: true,
	install_dir: plugins_install_dir,
)

gstptaudiobin = library('gstptaudiobin', 'gstptaudiobin.c',
	include_directories: conf_inc,
	dependencies: [gstreamer, gstreamer_audio],
	install: true,
	install_dir: plugins_install_dir,
)