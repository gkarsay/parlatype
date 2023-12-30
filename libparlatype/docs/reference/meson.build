version_conf = configuration_data()
version_conf.set('PACKAGE_VERSION', meson.project_version())
version_conf.set('API_VERSION', libparlatype_api_version)
version_conf.set('SONAME', libparlatype_version)
configure_file(
  input: 'parlatype-docs.xml.in',
  output: 'parlatype-docs.xml',
  configuration: version_conf,
)
configure_file(
  input: 'introduction.xml.in',
  output: 'introduction.xml',
  configuration: version_conf,
)
configure_file(
  input: 'using_library.xml.in',
  output: 'using_library.xml',
  configuration: version_conf,
)
configure_file(
  input: 'parlatype-example.py.in',
  output: 'parlatype-example.py',
  configuration: version_conf,
)

gnome.gtkdoc(
  'parlatype',
  main_xml: join_paths(meson.current_build_dir(), 'parlatype-docs.xml'),
  src_dir: [
    join_paths(meson.project_source_root(), 'libparlatype', 'src'),
    join_paths(meson.project_build_root(), 'libparlatype', 'src'),
  ],
  gobject_typesfile: join_paths(meson.current_source_dir(), 'libparlatype.types'),
  dependencies: libparlatype_dep,
  scan_args: [
    '--source-dir=' + join_paths(meson.project_source_root(), 'libparlatype', 'src'),
    '--rebuild-types',
    '--deprecated-guards="PT_DISABLE_DEPRECATED"',
  ],
  mkdb_args: [
    '--xml-mode',
    '--output-format=xml',
  ],
  content_files: [
    'gallery.xml',
    join_paths(meson.current_build_dir(), 'introduction.xml'),
    join_paths(meson.current_build_dir(), 'using_library.xml'),
    'python_example.xml',
    join_paths(meson.current_build_dir(), 'parlatype-example.py'),
    'parlatype-example.ui',
  ],
  ignore_headers: [
    'gnome-languages.h',
    'gst-helpers.h',
    'gstparlasphinx.h',
    'gstptaudioasrbin.h',
    'gstptaudiobin.h',
    'gstptaudioplaybin.h',
    'pt-i18n.h',
    'pt-position-manager.h',
    'pt-waveviewer-cursor.h',
    'pt-waveviewer-focus.h',
    'pt-waveviewer-scrollbox.h',
    'pt-waveviewer-selection.h',
    'pt-waveviewer-ruler.h',
    'pt-waveviewer-waveform.h',
  ],
  html_assets: [
    'images/pt-waveviewer.png',
  ],
  install: true,
)
