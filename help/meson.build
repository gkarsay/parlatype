media = [
	'figures/parlatype-icon.png',
	'figures/parlatype-prefs-asr.png',
	'figures/parlatype-prefs-controls.png',
	'figures/parlatype-prefs-timestamps.png',
	'figures/parlatype-prefs-waveform.png',
	'figures/parlatype-prefs-window.png',
	'figures/parlatype-main-window.png',
]

sources = [
	'index.page',
	'drag-and-drop.page',
	'first-steps.page',
	'keyboard-shortcuts.page',
	'legal.xml',
	'preferences.page',
	'selections.page',
	'timestamps.page',
]

if with_pocketsphinx or with_deepspeech
	sources += [
		'asr.page',
		'asr-setup.page',
	]
endif

if host_machine.system() != 'windows'
	sources += [
		'media-keys.page',
	]
endif

symlinks = true
if host_machine.system() == 'windows'
	symlinks = false
endif

gnome.yelp(
	app_id,
	media: media,
	sources: sources,
	symlink_media: symlinks
)
