# For information on how to properly maintain the library version information,
# refer to the libtool manual, section "Updating library version information":
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# The following string is 'current.revision.age':

libparlatype_version = '2.1.1'

# API or namespace version for GIR = first number of libparlatype_version + .0
libparlatype_api_version = '3.0'

gettext_package_lib = 'libparlatype'

gstreamer       = dependency('gstreamer-1.0', version : '>= 1.6.3')
gstreamer_app   = dependency('gstreamer-app-1.0')
gstreamer_audio = dependency('gstreamer-audio-1.0')
math = compiler.find_library('m', required: false)

subdir('data')
subdir('po')
subdir('src')
subdir('tests')
subdir('docs') # must be after src

if asr
	subdir('gst-plugin-sphinx')
endif