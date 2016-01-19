# Parlatype

## What is it?

Parlatype is a minimal audio player for manual speech transcription, written for the GNOME desktop environment. It plays audio sources to transcribe them in your favourite text application. You can control the speed of playback, playing slowly as you type, playing fast for editing. The pitch is not altered, there is no "chipmunk" effect. Whenever you pause playback, it will rewind a few seconds, so that you can easier resume. Of course you can change how much it rewinds or whether it rewinds at all.

Parlatype can be controled with the "Play" button from your multimedia keyboard. This way it doesn't have to have focus to control it. You can type in your text application and still have some (basic) control over Parlatype.


## What it's not

Parlatype is just an audio player, you still need another program, where you write your transcription to, e.g. Libre Office. It doesn't work with videos or streaming media. It's not a tool for scientific transcription, rather for your personal use. There is no speech recognition, you have to type yourself.

## Translations

Parlatype is fully translatable, currently there is an English and a German version. Any help in translations is welcome, please leave a message, if you're willing to contribute.

## Installation

### Dependencies

To install Parlatype from source you need the basic building infrastructure with make, autotools and intltool.
Then you need libgtk-3 (minimum version 3.10) and libgstreamer1.0.

On a Debian based distro you can install these with:

```
$ sudo apt-get install build-essential autotools-dev intltool libgtk-3-dev libgstreamer1.0-dev
```

In order to run it needs at least Gtk+ 3.10 and GStreamer 1.0 with the set of "good" plugins.
The Debian based command is:
```
$ sudo apt-get install libgtk-3-0 libgstreamer1.0-0 gstreamer1.0-plugins-good
```

### Building 
Building from git requires running autogen.sh:
```
$ git clone https://github.com/gkarsay/parlatype.git
$ cd parlatype
$ ./autogen.sh
$ make
$ sudo make install
```
Building from tarball: Download a release tarball, extract it and run:
```
$ ./configure
$ make
$ sudo make install
```


## Credits

Parlatype is based on Frederik Elwert's program *transcribe*.

