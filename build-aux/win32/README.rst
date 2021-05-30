===============================
Windows Installer Build Scripts
===============================

These scripts were taken from Quod Libet and adapted for Parlaype:

We use `msys2 <https://msys2.github.io/>`__ for creating the Windows installer
and development on Windows.


Setting Up the MSYS2 Environment
--------------------------------

* Download msys2 64-bit from https://msys2.github.io/
* Follow instructions on https://msys2.github.io/
* Execute ``C:\msys64\mingw64.exe``
* Run ``pacman -S git`` to install git
* Run ``git clone https://github.com/gkarsay/parlatype.git``
* Run ``cd parlatype/build-aux/win32`` to end up where this README exists.
* Execute ``./bootstrap.sh`` to install all the needed dependencies.
* Now you can compile Parlatype like on Linux


Creating an Installer
---------------------

Simply run ``./build.sh [git-tag]`` and the installer should appear in this
directory.

You can pass a git tag ``./build.sh v1.7`` to build a specific tag or
pass nothing to build main. Note that it will clone from this repository and
not from github so any commits present locally will be cloned as well.