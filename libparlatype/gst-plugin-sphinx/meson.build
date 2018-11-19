sphinxbase = dependency('sphinxbase')
pocketsphinx = dependency('pocketsphinx')
plugins_install_dir = '@0@/gstreamer-1.0'.format(get_option('libdir'))

gstcutter = library('gst-plugin-sphinx', 'gstpocketsphinx.c',
	include_directories: conf_inc,
	dependencies: [sphinxbase, pocketsphinx, gstreamer, gstreamer_audio],
	install: true,
	install_dir: plugins_install_dir,
)
