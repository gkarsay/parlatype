#!/usr/bin/env bash
# Copyright 2016 Christoph Reiter
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Taken from Quod Libet and modified for Parlatype by Gabor Karsay.

set -e

function main {

    if [[ "$MSYSTEM" == "MINGW32" ]]; then
        local MSYS2_ARCH="i686"
    else
        local MSYS2_ARCH="x86_64"
    fi

    pacman --noconfirm -Suy

    pacman --noconfirm -S --needed \
        git \
        mingw-w64-$MSYS2_ARCH-meson \
        mingw-w64-$MSYS2_ARCH-gcc \
        mingw-w64-$MSYS2_ARCH-pkg-config \
        mingw-w64-$MSYS2_ARCH-gtk3 \
        mingw-w64-$MSYS2_ARCH-yelp-tools \
        mingw-w64-$MSYS2_ARCH-gstreamer \
        mingw-w64-$MSYS2_ARCH-gst-plugins-base \
        mingw-w64-$MSYS2_ARCH-gst-plugins-good
}

main;