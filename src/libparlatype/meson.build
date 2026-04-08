# These headers will be installed
libparlatype_public_headers = [
  'parlatype.h',
  'pt-app.h',
  'pt-asr-dialog.h',
  'pt-asr-manager.h',
  'pt-audio-controls.h',
  'pt-config.h',
  'pt-config-list.h',
  'pt-config-row.h',
  'pt-file-loader.h',
  'pt-file-saver.h',
  'pt-media-info.h',
  'pt-playback-rate-button.h',
  'pt-player.h',
  'pt-position-manager.h',
  'pt-preferences.h',
  'pt-prefs-info-row.h',
  'pt-prefs-install-row.h',
  'pt-speech-recognizer.h',
  'pt-text-buffer.h',
  'pt-text-view.h',
  'pt-timestamp.h',
  'pt-timestamp-button.h',
  'pt-utils.h',
  'pt-volume-button.h',
  'pt-waveloader.h',
  'pt-waveviewer.h',
  'pt-window.h',
  'pt-window-addin.h',
]

# These headers are to be ignored by gi-docgen
libparlatype_private_headers = [
  # in contrib/
  'contrib/gnome-languages.h',
  # in gst/
  'gst/gst-helpers.h',
  'gst/gstptaudiobin.h',
  'gst/gstptaudioasrbin.h',
  'gst/gstptaudioplaybin.h',
  # in ./
  'editor-theme-selector-private.h',
  'pt-goto-dialog.h',
  'pt-i18n.h',
  'pt-media-info-private.h',
  'pt-save-changes-dialog.h',
  'pt-waveviewer-cursor.h',
  'pt-waveviewer-ruler.h',
  'pt-waveviewer-scrollbox.h',
  'pt-waveviewer-selection.h',
  'pt-waveviewer-waveform.h',
  'pt-window-dnd.h',
  'pt-window-private.h',
]

install_headers(libparlatype_public_headers, subdir: 'parlatype')

# These sources will be introspected
libparlatype_public_sources = [
  'pt-app.c',
  'pt-asr-dialog.c',
  'pt-asr-manager.c',
  'pt-audio-controls.c',
  'pt-config.c',
  'pt-config-list.c',
  'pt-config-row.c',
  'pt-file-loader.c',
  'pt-file-saver.c',
  'pt-media-info.c',
  'pt-playback-rate-button.c',
  'pt-player.c',
  'pt-position-manager.c',
  'pt-preferences.c',
  'pt-prefs-info-row.c',
  'pt-prefs-install-row.c',
  'pt-speech-recognizer.c',
  'pt-text-buffer.c',
  'pt-text-view.c',
  'pt-timestamp.c',
  'pt-timestamp-button.c',
  'pt-utils.c',
  'pt-volume-button.c',
  'pt-waveloader.c',
  'pt-waveviewer.c',
  'pt-window.c',
  'pt-window-addin.c',
]

# These are private and not for introspection
libparlatype_private_sources = [
  'contrib/gnome-languages.c',
  'gst/gst-helpers.c',
  'gst/gstptaudiobin.c',
  'gst/gstptaudioasrbin.c',
  'gst/gstptaudioplaybin.c',
  'editor-theme-selector.c',
  'pt-goto-dialog.c',
  'pt-i18n.c',
  'pt-save-changes-dialog.c',
  'pt-waveviewer-cursor.c',
  'pt-waveviewer-ruler.c',
  'pt-waveviewer-scrollbox.c',
  'pt-waveviewer-selection.c',
  'pt-waveviewer-waveform.c',
  'pt-window-dnd.c',
]

libparlatype_resources = gnome.compile_resources('pt-lib-resources',
  'resources/libparlatype.gresource.xml',
          source_dir: 'resources',
              c_name: '_pt_lib',
)

marshalers = gnome.genmarshal('pt-marshalers',
            internal: true,
              prefix: '_pt_cclosure_marshal',
             sources: 'ptmarshalers.list',
  valist_marshallers: true,
)

libparlatype_generated_headers = [
  libparlatype_resources[1],
]

libparlatype_sources = [
  libparlatype_generated_headers,
  libparlatype_public_sources,
  libparlatype_private_sources,
  libparlatype_resources,
  marshalers,
]

libparlatype_deps = [
  config_h,
  gtk,
  gstreamer,
  gstreamer_app,
  gstreamer_audio,
  math,
  libadwaita,
  libxml,
  libpeas,
  girepository,
]

# Static library
libparlatype_static = static_library('parlatype-static', libparlatype_sources,
               c_args: ['-DPARLATYPE_COMPILATION'],
         dependencies: libparlatype_deps,
  include_directories: [include_directories('..')],
)

libparlatype_static_dep = declare_dependency(
              sources: libparlatype_private_headers + libparlatype_generated_headers,
         dependencies: libparlatype_deps,
            link_with: libparlatype_static,
  include_directories: include_directories('.'),
)

parlatype_public_sources += files(libparlatype_public_sources)
parlatype_public_headers += files(libparlatype_public_headers)
parlatype_private_headers += files(libparlatype_private_headers)
parlatype_generated_headers += libparlatype_generated_headers
#parlatype_include_subdirs += libparlatype_header_subdir
#parlatype_gir_extra_args += '-DIDE_CORE_COMPILATION'
parlatype_main_headers += 'parlatype.h'
