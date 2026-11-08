# These are updated by subdir() meson.build files so that we
# can generate gir files and what not appropriate for the
# final binary (which statically links their .a libraries)
parlatype_include_subdirs = []
parlatype_public_sources = []
parlatype_public_headers = []
parlatype_private_sources = []
parlatype_private_headers = []
parlatype_generated_sources = []
parlatype_generated_headers = []
parlatype_gir_extra_args = []
parlatype_main_headers = []

subdir('libparlatype')
subdir('plugins')

parlatype_deps = [
  config_h,
  gtk,
  libadwaita,
  libxml,
  math,
  gstreamer,
  libparlatype_static_dep,
]

parlatype_static = [
  libparlatype_static,
  plugins,
]

parlatype = executable('parlatype', 'main.c',
              install: true,
           link_whole: parlatype_static,
         dependencies: parlatype_deps,
       export_dynamic: true,
)

pkgconfig = import('pkgconfig')

gir = get_option('gir')

if gir
  gir_dep = dependency('gobject-introspection-1.0')

  parlatype_gir_extr_args = [
    '-DPARLATYPE_COMPILATION',
  ]

parlatype_pkg_requires = [
  'gio-2.0',
  'gtk4',
  'libpeas-2',
]

#parlatype_gir_includes = [
#  'Adw-1',
#  'Gio-2.0',
#  'Gtk-4.0',
#  'Peas-2',
#]

# We use requires: instead of libraries: so that our link args of
# things like -Wl,--require-defined= do not leak into the .pc file.
#pkgconfig.generate(
#      subdirs: parlatype_include_subdirs,
#      version: meson.project_version(),
#         name: 'parlatype-@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION),
#     filebase: 'parlatype-@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION),
#  description: 'Contains the plugin container for Parlatype.',
#  install_dir: join_paths(get_option('libdir'), 'pkgconfig'),
#     requires: parlatype_pkg_requires,
#)

libparlatype_gir = gnome.generate_gir(parlatype,
              sources: parlatype_generated_headers +
                       parlatype_generated_sources +
                       parlatype_public_headers +
                       parlatype_public_sources,
            nsversion: libparlatype_api_version,
            namespace: 'Pt',
        symbol_prefix: 'pt_',
    identifier_prefix: 'Pt',
             includes: ['GObject-2.0', 'Gtk-4.0', 'Gst-1.0'],
               header: parlatype_main_headers,
              install: true,
           extra_args: parlatype_gir_extr_args,
  )
endif

subdir('tests')