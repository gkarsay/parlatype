# For information on how to properly maintain the library version information,
# refer to the libtool manual, section "Updating library version information":
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html

libparlatype_version = '2.0.0'

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

subdir('gst-plugin-sphinx')
