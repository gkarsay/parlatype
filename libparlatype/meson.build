# For information on how to properly maintain the library version information,
# refer to the libtool manual, section "Updating library version information":
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html

libparlatype_current = 4
libparlatype_revision = 0
libparlatype_age = 1
libparlatype_current_minus_age = libparlatype_current - libparlatype_age

libparlatype_version = '@0@.@1@.@2@'.format(libparlatype_current_minus_age, libparlatype_age, libparlatype_revision)

# API or namespace version for GIR 'x.0'
libparlatype_api_version = '3.0'

gstreamer       = dependency('gstreamer-1.0', version : '>= 1.6.3')
gstreamer_app   = dependency('gstreamer-app-1.0')
gstreamer_audio = dependency('gstreamer-audio-1.0')
gnome_desktop   = dependency('gnome-desktop-3.0')
math = compiler.find_library('m', required: false)

subdir('data')
subdir('po')
subdir('src')
subdir('tests')
subdir('docs') # must be after src