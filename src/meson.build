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
	'pt-asr-output.c',
	'pt-asr-output.h',
	'pt-dbus-service.c',
	'pt-dbus-service.h',
	'pt-goto-dialog.c',
	'pt-goto-dialog.h',
	'pt-mediakeys.c',
	'pt-mediakeys.h',
	'pt-preferences.c',
	'pt-preferences.h',
	'pt-window.c',
	'pt-window.h',
	'pt-window-dnd.c',
	'pt-window-dnd.h',
	'pt-window-private.h',
]

executable('parlatype',
	src,
	link_with: libparlatype,
	include_directories : conf_inc,
	install: true,
	dependencies: [ gtk, gmodule, atspi ],
)
