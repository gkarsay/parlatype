tests = [
	'config',
	'gst',
	'player',
	'waveloader',
	'waveviewer',
]

test_data = [
	'data/test1.ogg',
]

extra_sources = [
	'../src/gst/gstptaudiobin.c',
	'../src/gst/gstptaudiobin.h',
	'../src/gst/gstptaudioasrbin.c',
	'../src/gst/gstptaudioasrbin.h',
	'../src/gst/gstptaudioplaybin.c',
	'../src/gst/gstptaudioplaybin.h',
	'mock-plugin.c',
	'mock-plugin.h',
]

gen_prog = find_program('data/generate_config_data', required: true)
run_command(gen_prog)

foreach t: tests
	exe = executable(
		t, '@0@.c'.format(t), extra_sources,
		install: false,
		dependencies: [ libparlatype_dep, gstreamer, gstreamer_audio, gnome_desktop ],
	)

	test(
		t, exe,
		args: [ '--tap', '-k' ],
		env: [
			'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
			'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
			'LANGUAGE=C',
		],
	)
endforeach