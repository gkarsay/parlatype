---
layout: page
title: Features
---

## Waveform
The waveform makes it easy to navigate in your audio file. You see what comes next and spot silence.

## Adjustable speed
You can control the speed of playback, playing slowly as you type, playing fast for editing. The pitch is not altered, there is no “chipmunk” effect.

## Rewind on pause
Whenever you pause playback, it will rewind a few seconds, so that you can easier resume. Of course you can change how much it rewinds or whether it rewinds at all.

## Timestamps
Parlatype produces timestamps which you can insert in your transcription. Parlatype will jump to that position at your will (drag 'n' drop or with LibreOffice Helpers).

## LibreOffice Extension
Parlatype recommends to use LibreOffice. An extension lets you link a media file to a document and jump to timestamps. A set of macros can be assigned to key bindings, e.g. to insert timestamps.

## Automatic speech recognition
This is a working feature (since version 1.6), however, it’s hidden by default (since version 1.6.1).

You have to find and download speech model data for your language. This step is described in the help pages but I can’t give any support beyond that.

The results for general speech recognition are not overwhelming with the current ASR engine and given the fact that many languages are missing speech model data, this feature is only shown, if Parlatype is run using the option `--with-asr`.

Distributions may decide to compile Parlatype without this feature.

## Plays almost every audio file
Parlatype is using the GStreamer framework which supports – with plugins – almost any audio file on your disk. Streaming media is not supported, you have to download it first.

## Media keys and foot pedals
Parlatype can be controlled with the “Play” button from your multimedia keyboard. This way it doesn’t have to have focus to control it. You can type in your text application and still have some (basic) control over Parlatype. Foot pedals can be assigned to the play button.

### Footpedals
This subsection documents how to get various footpedals to play nicely with Parlatype. Go to the [footpedals](footpedals/footpedals.md) section for details.

## Start on top
Parlatype can start on top of other windows. If you are working with a maximized text application, you can still see the Parlatype window.

## Help pages
Parlatype is easy to use nevertheless everything is documented in the help pages. You can see the [English help online](help-online/index.html).

## International
The user interface is fully translatable. Currently the program (without help pages) is available fully or almost fully translated in English, British English, Catalan, Czech, Dutch, Finnish, French, German, Hungarian, Indonesian, Italian, Japanese, Lithuanian, Polish, Spanish and Swedish. It's partly translated into Portuguese, Malay, Kurdish, Serbian, Kabyle, Arabic and Latvian.

Any help in translations is welcome! Parlatype is translated at [https://translations.launchpad.net/parlatype](https://translations.launchpad.net/parlatype).

## For developers
Parlatype ships its own library, libparlatype, which provides a GStreamer backend (PtPlayer) and a waveviewer widget (PtWaveviewer) which is a GtkWidget. It is fully documented, however the API is not stable yet. See the [reference online](reference/index.html).

