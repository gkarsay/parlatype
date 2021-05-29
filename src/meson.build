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
	'pt-asr-dialog.c',
	'pt-asr-dialog.h',
	'pt-config-row.c',
	'pt-config-row.h',
	'pt-controller.c',
	'pt-controller.h',
	'pt-goto-dialog.c',
	'pt-goto-dialog.h',
	'pt-preferences.c',
	'pt-preferences.h',
	'pt-prefs-asr.c',
	'pt-prefs-asr.h',
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
	'pt-win32-helpers.c',
	'pt-win32-helpers.h',
	'pt-win32-hotkeys.c',
	'pt-win32-hotkeys.h',
	'pt-win32-pipe.c',
	'pt-win32-pipe.h',
	]
endif

parlatype_deps = [
	config_h,
	gtk,
]

executable('parlatype',
	src,
	link_with: libparlatype,
	include_directories : conf_inc,
	install: true,
	dependencies: parlatype_deps,
	gui_app: true,
	c_args: ['-DG_LOG_DOMAIN="parlatype"' ],
)