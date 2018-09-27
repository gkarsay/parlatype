libparlatype_so_version = '1.6.0'
# Parlatype is unstable, we don't use API version yet
# libparlatype_api_version = '1.0'

gettext_package_lib = 'libparlatype'

gstreamer       = dependency('gstreamer-1.0', version : '>= 1.6.3')
gstreamer_audio = dependency('gstreamer-audio-1.0')
compiler = meson.get_compiler('c')
math = compiler.find_library('m', required: false)

subdir('data')
subdir('po')
subdir('src')
subdir('tests')
subdir('docs') # must be after src
