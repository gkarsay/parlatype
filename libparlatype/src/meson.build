gresources = gnome.compile_resources(
  'pt-lib-resources',
  'resources/libparlatype.gresource.xml',
  source_dir: 'resources',
  c_name: '_pt',
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
  'pt-config.c',
  'pt-player.c',
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
  'pt-i18n.c',
  'pt-position-manager.c',
  'pt-waveviewer-cursor.c',
  'pt-waveviewer-ruler.c',
  'pt-waveviewer-scrollbox.c',
  'pt-waveviewer-selection.c',
  'pt-waveviewer-waveform.c',
]

# These headers will be installed
public_headers = [
  'parlatype.h',
  'pt-config.h',
  'pt-player.h',
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
  'pt-position-manager.h',
  'pt-waveviewer-cursor.h',
  'pt-waveviewer-ruler.h',
  'pt-waveviewer-scrollbox.h',
  'pt-waveviewer-selection.h',
  'pt-waveviewer-waveform.h',
]

deps = [
  gtk,
  gstreamer,
  gstreamer_app,
  gstreamer_audio,
  math,
]

if with_pocketsphinx_legacy
  private_sources += 'gst/gstparlasphinx-legacy.c'
  sphinxbase   = dependency('sphinxbase')
  pocketsphinx = dependency('pocketsphinx')
  deps += [sphinxbase, pocketsphinx]
endif

if with_pocketsphinx
  private_sources += 'gst/gstparlasphinx.c'
  pocketsphinx = dependency('pocketsphinx', version: '>= 5.0')
  deps += [pocketsphinx]
endif


install_headers(public_headers, subdir: 'parlatype')

mapfile = 'libparlatype.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
libparlatype = shared_library(
  'parlatype',
  sources: [ public_sources, private_sources ],
  version: libparlatype_version,
  dependencies: deps,
  include_directories: libconf_inc,
  link_args: vflag,
  link_depends: mapfile,
  install: true,
  c_args: [
    '-DPARLATYPE_COMPILATION',
    '-DG_LOG_DOMAIN="libparlatype"',
  ],
)

libparlatype_dep = declare_dependency(
  link_with: libparlatype,
  include_directories: libconf_inc,
  dependencies: [ gtk, ],
)

pkg = import('pkgconfig')

pkg.generate(
  libparlatype,
  filebase: meson.project_name(),
  name: meson.project_name(),
  description: 'libparlatype, core support for parlatype',
  version: meson.project_version(),
  subdirs: meson.project_name(),
  requires: [
    'gtk4',
  ],
  requires_private: [
    'gstreamer-1.0',
  ],
)

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
