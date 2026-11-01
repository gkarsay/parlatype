#!/usr/bin/env bash

rm -f xyz.parlatype.Parlatype.flatpak
rm -rf _build ; mkdir _build
rm -rf _repo ; mkdir _repo

STATE_DIR=~/.cache/parlatype/flatpak-builder
BRANCH=pt5
MANIFEST=Parlatype-Whisper1.8.4.json

flatpak-builder \
    --ccache --force-clean \
    --repo=_repo --state-dir=$STATE_DIR \
    --default-branch=$BRANCH \
    _build $MANIFEST

flatpak build-bundle \
    _repo xyz.parlatype.Parlatype.flatpak xyz.parlatype.Parlatype $BRANCH
