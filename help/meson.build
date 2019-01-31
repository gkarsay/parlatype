media = [
	'figures/parlatype-icon.png',
	'figures/parlatype-prefs-controls.png',
	'figures/parlatype-prefs-timestamps.png',
	'figures/parlatype-prefs-waveform.png',
	'figures/parlatype-prefs-window.png',
	'figures/parlatype-main-window.png',
]

sources = [
	'index.page',
	'asr.page',
	'asr-setup.page',
	'drag-and-drop.page',
	'first-steps.page',
	'keyboard-shortcuts.page',
	'legal.xml',
	'media-keys.page',
	'preferences.page',
	'selections.page',
	'timestamps.page',
]

if libreoffice
	media += [
		'figures/lo-customize.png',
		'figures/lo-menu.png',
	]
	sources += [
		'libreoffice-helpers.page',
	]
endif

gnome.yelp(
	app_id,
	media: media,
	sources: sources,
)
