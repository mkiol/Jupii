# Jupii

Linux desktop and Sailfish OS app for playing multimedia content on UPnP/DLNA devices

## Contents of this README

- [Description](#description)
- [How to install](#how-to-install)
- [Building from sources](#building-from-sources)
- [Contributing to Jupii](#contributing-to-jupii)
- [License](#license)

## Description

**Jupii** let you play audio, video and image files on any device on your local network that supports UPnP/DLNA.

Here's a closer look at what it can do:

- **Device Discovery:** Auto detect and connect with DLNA devices present in your local network, such as smart speaker, smart TVs, gaming consoles, and more
- **Remote Control:** Play, pause, skip, seek, and adjust volume
- **Local Content Playback:** Share your locally stored music, videos, and images with DLNA device on your network
- **Internet Radio & Streaming:** Play multimedia content from online services such as: Internet radio, SomaFM channels, Icecast streams, FOSDEM videos, Bandcamp, SoundCloud, TuneIn stations, YouTube Music, radio.net
- **Media Server Access:** Browse and play media files stored on UPnP Media Servers within your network
- **Camera & Microphone Casting:** Share real-time camera and microphone output
- **Audio Playback Streaming:** Capture local audio playback and share it with DLNA device
- **Screen Casting:** Mirror your device's screen
- **Music Track Recording:** Capture and save your favorite music tracks, including those from Icecast streams, so you can listen to them offline
- **Play Queue & Playlists:** Organize your media playback with customizable play queues and playlists

## How to install

- Sailfish OS: [OpenRepos](https://openrepos.net/content/mkiol/jupii)
- Flatpack packages with KDE Plasma UI: [Releases](https://github.com/mkiol/Jupii/releases)

## Building from sources

### Flatpak

```
git clone https://github.com/mkiol/Jupii.git

cd Jupii/flatpak

flatpak-builder --user --install-deps-from=flathub --repo="/path/to/local/flatpak/repo" "/path/to/output/dir" net.mkiol.Jupii.yaml
```

### Sailfish OS

```
git clone https://github.com/mkiol/Jupii.git

cd Jupii
mkdir build
cd build

sfdk config --session specfile=../sfos/harbour-jupii.spec
sfdk config --session target=SailfishOS-4.4.0.58-aarch64
sfdk cmake ../ -DCMAKE_BUILD_TYPE=Release -DWITH_SFOS=ON
sfdk package
```

## Contributing to Jupii

Any contribution is very welcome!

Feel free to make a PR, report an issue or reqest for new feature.

### Translations

Translation files in Qt format are in [translations dir](https://github.com/mkiol/Jupii/tree/master/translations).

Preferred way to contribute translation is via [Transifex service](https://www.transifex.com/mkiol/jupii),
but if you would like to make a direct PR, please do it.

## Libraries

**Jupii** relies on following open source projects:

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

**Jupii** is an open source project. Source code is released under the
[Mozilla Public License Version 2.0](https://www.mozilla.org/MPL/2.0/).

