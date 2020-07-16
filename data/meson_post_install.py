#!/usr/bin/env python3

import os
import sys
import subprocess

install_prefix = os.environ['MESON_INSTALL_PREFIX']
icondir = os.path.join(install_prefix, 'share', 'icons', 'hicolor')
schemadir = os.path.join(install_prefix, 'share', 'glib-2.0', 'schemas')
if sys.platform == 'win32':
    icon_updater = 'gtk-update-icon-cache-3.0.exe'
else:
    icon_updater = 'gtk-update-icon-cache'


if not os.environ.get('DESTDIR'):
    print('Update icon cache...')
    subprocess.call([icon_updater,
                     '--force',
                     '--ignore-theme-index',
                     icondir])

    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', schemadir])
