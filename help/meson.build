media = [
	'figures/parlatype-icon.png',
	'figures/parlatype-prefs-controls.png',
	'figures/parlatype-prefs-timestamps.png',
	'figures/parlatype-prefs-waveform.png',
	'figures/parlatype-prefs-window.png',
	'figures/parlatype-main-window.png',
]

# GNOME icons
media += [
	'figures/action-unavailable-symbolic.svg',
	'figures/emblem-system-symbolic.svg',
	'figures/folder-download-symbolic.svg',
	'figures/object-select-symbolic.svg',
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

if with_asr
	media += [
		'figures/asr-setup-initial.png',
		'figures/asr-setup-downloadable.png',
		'figures/asr-setup-details.png',
		'figures/asr-switch-to-asr.png',
	]
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
