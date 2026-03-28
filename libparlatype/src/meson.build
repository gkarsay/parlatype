gresources = gnome.compile_resources(
  'pt-lib-resources',
  'resources/libparlatype.gresource.xml',
  source_dir: 'resources',
  c_name: '_pt_lib',
)

marshalers = gnome.genmarshal(
  'pt-marshalers',
  sources: 'ptmarshalers.list',
  prefix: '_pt_cclosure_marshal',
  valist_marshallers: true,
)

marshal_h = marshalers[1]

# These sources will be introspected
public_sources = [
  'pt-asr-manager.c',
  'pt-asr-iface.c',
  'pt-config.c',
  'pt-file-saver.c',
  'pt-media-info.c',
  'pt-player.c',
  'pt-text-view.c',
  'pt-waveloader.c',
  'pt-waveviewer.c',
]

# These are private and not for introspection
private_sources = [
  gresources,
  marshalers,
  'contrib/gnome-languages.c',
  'gst/gst-helpers.c',
  'gst/gstptaudiobin.c',
  'gst/gstptaudioasrbin.c',
  'gst/gstptaudioplaybin.c',
  'pt-file-loader.c',
  'pt-i18n.c',
  'pt-position-manager.c',
  'pt-text-buffer.c',
  'pt-timestamp-button.c',
  'pt-waveviewer-cursor.c',
  'pt-waveviewer-ruler.c',
  'pt-waveviewer-scrollbox.c',
  'pt-waveviewer-selection.c',
  'pt-waveviewer-waveform.c',
]

# These headers will be installed
public_headers = [
  'parlatype.h',
  'pt-asr-manager.h',
  'pt-asr-iface.h',
  'pt-config.h',
  'pt-file-saver.h',
  'pt-media-info.h',
  'pt-player.h',
  'pt-text-view.h',
  'pt-waveloader.h',
  'pt-waveviewer.h',
]

# These headers are to be ignored by gtk-doc
private_headers = [
  # in contrib/
  'gnome-languages.h',
  # in gst/
  'gst-helpers.h',
  'gstparlasphinx-legacy.h',
  'gstparlasphinx.h',
  'gstptaudiobin.h',
  'gstptaudioasrbin.h',
  'gstptaudioplaybin.h',
  # in ./
  'pt-i18n.h',
  'pt-media-info-private.h',
  'pt-position-manager.h',
  'pt-waveviewer-cursor.h',
  'pt-waveviewer-ruler.h',
  'pt-waveviewer-scrollbox.h',
  'pt-waveviewer-selection.h',
  'pt-waveviewer-waveform.h',
]

libparlatype_deps = [
  gtk,
  gstreamer,
  gstreamer_app,
  gstreamer_audio,
  math,
  libadwaita,
  libxml,
  libpeas,
]

install_headers(public_headers, subdir: 'parlatype')

# Static library
libparlatype_static = static_library(
  'parlatype-static',
  sources: [ public_sources, private_sources ],
  include_directories: libconf_inc,
  dependencies: libparlatype_deps,
  c_args: [
    '-DPARLATYPE_COMPILATION',
    '-DG_LOG_DOMAIN="libparlatype"',
  ],
)

libparlatype_static_dep = declare_dependency(
  link_with: libparlatype_static,
  include_directories: libconf_inc,
  dependencies: libparlatype_deps,
)

pkg = import('pkgconfig')

gir = get_option('gir')

if gir
  gir_dep = dependency('gobject-introspection-1.0')
  introspection_sources = [
    public_headers,
    public_sources,
  ]

  gir_args = [
    '-DPARLATYPE_COMPILATION',
    '--quiet',
  ]

  gnome.generate_gir(libparlatype,
    sources: introspection_sources,
    nsversion: libparlatype_api_version,
    namespace: 'Parlatype',
    symbol_prefix: 'pt_',
    identifier_prefix: 'Pt',
    includes: ['GObject-2.0', 'Gtk-4.0'],
    install: true,
    extra_args: gir_args,
  )
endif

subdir('plugins')