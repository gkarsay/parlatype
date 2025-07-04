# For information on how to properly maintain the library version information,
# refer to the libtool manual, section "Updating library version information":
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html

libparlatype_current = 7
libparlatype_revision = 3
libparlatype_age = 0
libparlatype_current_minus_age = libparlatype_current - libparlatype_age

libparlatype_version = '@0@.@1@.@2@'.format(libparlatype_current_minus_age, libparlatype_age, libparlatype_revision)

# API or namespace version
# Increment with every API break (changed or removed symbols)
# For x this will be used as 'x.0', the zero never changes
libparlatype_api = 5

libparlatype_api_version = '@0@.0'.format(libparlatype_api)

gstreamer       = dependency('gstreamer-1.0', version : '>= 1.6.3')
gstreamer_app   = dependency('gstreamer-app-1.0')
gstreamer_audio = dependency('gstreamer-audio-1.0')
math = compiler.find_library('m', required: false)

# library message catalog is soname versioned for parallel installs
gettext_package_lib = 'libparlatype@0@'.format(libparlatype_current_minus_age)

iso_codes = dependency('iso-codes')
iso_codes_prefix = iso_codes.get_variable(pkgconfig: 'prefix')

libconf = configuration_data()
libconf.set_quoted('PACKAGE_VERSION', meson.project_version())
libconf.set_quoted('PACKAGE_URL', 'https://www.parlatype.xyz')
libconf.set_quoted('GETTEXT_LIB', gettext_package_lib)
libconf.set_quoted('ISO_CODES_PREFIX', iso_codes_prefix)
libconf.set('HAVE_POCKETSPHINX_LEGACY', with_pocketsphinx_legacy)
libconf.set('HAVE_POCKETSPHINX', with_pocketsphinx)
libconf_input = configure_file(output: 'config.h', configuration: libconf)
libconf_inc = include_directories('.', 'src')


subdir('po')
subdir('src')
subdir('tests')
subdir('docs') # must be after src
