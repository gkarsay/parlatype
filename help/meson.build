media = [
  'figures/parlatype-icon.png',
  'figures/parlatype-prefs-controls.png',
  'figures/parlatype-prefs-timestamps.png',
  'figures/parlatype-prefs-waveform.png',
  'figures/parlatype-main-window.png',
]

sources = [
  'index.page',
  'drag-and-drop.page',
  'first-steps.page',
  'keyboard-shortcuts.page',
  'legal.xml',
  'media-keys.page',
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
    # GNOME icons
    'figures/action-unavailable-symbolic.svg',
    'figures/emblem-system-symbolic.svg',
    'figures/folder-download-symbolic.svg',
    'figures/object-select-symbolic.svg',
  ]
  sources += [
    'asr.page',
    'asr-setup.page',
  ]
endif

gnome.yelp(
  app_id,
  media: media,
  sources: sources,
  # generated screenshots not shown with symlinks
  symlink_media: false,
)
