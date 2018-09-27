gresources = gnome.compile_resources(
	'parlatype-resources', 'resources/parlatype.gresource.xml',
	source_dir : 'resources',
	#c-name : 'parlatype',
)

src = [
	gresources,
	'main.c',
	'pt-app.c',
	'pt-app.h',
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
	dependencies: [ gtk, gmodule ],
)
