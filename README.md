# Jupii

UPnP/DLNA client for Sailfish OS

## Features

- Discovery of UPnP/DLNA devices in a local network
- Remote control (Play, Pause, Next, Prev, Seek, Volume up/down)
- Playing local content (Music, Video, Images) on Media Renderer devices
- Playing gPodder's podcasts
- Playing remote content (e.g. internet radio, SomaFM channels, Icecast streams, FOSDEM videos, Bandcamp, SoundCloud, TuneIn stations, YouTube Music)
- Playing items from Media Servers on Media Renderer devices
- Streaming of local Microphone to Media Renderer devices
- Streaming of audio playback of any local application
- Screen casting
- Recording of music tracks (including tracks in Icecast streams)
- Play queue (play once/repeat options)
- Playlists (saving/opening)
- Sharing content to other devices via UPnP Media Server

## Download

- Sailfish OS: [OpenRepos](https://openrepos.net/content/mkiol/jupii)
- (experimental) Flatpack packages with Plasma UI: [Releases](https://github.com/mkiol/Jupii/releases)

## Building from sources

### Sailfish OS

```
git clone https://github.com/mkiol/Jupii.git

cd Jupii
mkdir build
cd build

sfdk config --session specfile=../../sfos/harbour-jupii.spec
sfdk config --session target=SailfishOS-4.4.0.58-aarch64
sfdk cmake ../ -DCMAKE_BUILD_TYPE=Release -Dwith_sfos=1
sfdk package
```

### Flatpak

```
git clone https://github.com/mkiol/Jupii.git

cd Jupii/flatpak

flatpak-builder --user --install-deps-from=flathub --repo=~/flatpak ../build org.mkiol.Jupii.yaml
flatpak build-bundle ~/flatpak jupii.flatpak org.mkiol.Jupii stable
```

## D-Bus API

App exposes simple [D-Bus service](https://github.com/mkiol/Jupii/blob/master/dbus/jupii.xml).
It can be used to make integration with other applications.

## Translations

Enabled translations are [here](https://github.com/mkiol/Jupii/tree/master/translations).

Every new translation is very welcome. There are three ways to contribute:

- [Transifex project](https://www.transifex.com/mkiol/jupii) (preferred)
- Direct github pull request
- Translation file sent to me via [e-mail](mailto:jupii@mkiol.net)

## Libraries

Jupii relies on following open source projects:

- [Qt](https://www.qt.io/)
- [Kirigami2](https://api.kde.org/frameworks/kirigami/html/index.html)
- [QHTTPServer](https://github.com/nikhilm/qhttpserver)
- [npupnp](https://framagit.org/medoc92/npupnp)
- [Libupnpp](https://framagit.org/medoc92/libupnpp)
- [TagLib](http://taglib.org/)
- [FFmpeg](https://ffmpeg.org/)
- [Lame](https://lame.sourceforge.io/)
- [x264](https://www.videolan.org/developers/x264.html)
- [Gumbo](https://github.com/google/gumbo-parser)
- [yt-dlp](https://github.com/yt-dlp/yt-dlp)
- [ytmusicapi](https://github.com/sigma67/ytmusicapi)
- [EasyEXIF](https://github.com/mayanklahiri/easyexif)
- [AudioTube](https://github.com/KDE/audiotube)
- [{fmt}](https://fmt.dev)

## License

Jupii is developed as an open source project under [Mozilla Public License Version 2.0](https://www.mozilla.org/MPL/2.0/).
