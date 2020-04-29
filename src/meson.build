gresources = gnome.compile_resources(
	'pt-win-resources',
	'resources/parlatype.gresource.xml',
	source_dir: 'resources',
	c_name: '_pt',
)

src = [
	gresources,
	'main.c',
	'pt-app.c',
	'pt-app.h',
	'pt-asr-assistant.c',
	'pt-asr-assistant.h',
	'pt-asr-assistant-helpers.c',
	'pt-asr-assistant-helpers.h',
	'pt-asr-settings.c',
	'pt-asr-settings.h',
	'pt-controller.c',
	'pt-controller.h',
	'pt-goto-dialog.c',
	'pt-goto-dialog.h',
	'pt-preferences.c',
	'pt-preferences.h',
	'pt-window.c',
	'pt-window.h',
	'pt-window-dnd.c',
	'pt-window-dnd.h',
	'pt-window-private.h',
]

if host_machine.system() != 'windows'
	src += [
	'pt-dbus-service.c',
	'pt-dbus-service.h',
	'pt-mediakeys.c',
	'pt-mediakeys.h',
	]
endif

if host_machine.system() == 'windows'
	rc = configure_file(
		input: 'resources/parlatype.rc.in',
		output: '@BASELINE@',
		configuration: rc_conf,
	)

	src += [
	windows.compile_resources(rc),
	'pt-win32-datacopy.c',
	'pt-win32-datacopy.h',
	]
endif

parlatype_deps = [
	gtk,
]

if asr
	src += [
	'pt-asr-output.c',
	'pt-asr-output.h',
	]
	parlatype_deps += dependency('atspi-2')
endif

executable('parlatype',
	src,
	link_with: libparlatype,
	include_directories : conf_inc,
	install: true,
	dependencies: parlatype_deps,
	gui_app: true,
)