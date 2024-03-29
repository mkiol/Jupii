# Jupii

Linux desktop and Sailfish OS app for playing multimedia on UPnP/DLNA devices

<a href='https://flathub.org/apps/net.mkiol.Jupii'><img width='240' alt='Download on Flathub' src='https://dl.flathub.org/assets/badges/flathub-badge-en.png'/></a>

## Contents of this README

- [Description](#description)
- [How to install](#how-to-install)
- [Building from sources](#building-from-sources)
- [Contributing to Jupii](#contributing-to-jupii)
- [How to support](#how-to-support)
- [License](#license)

## Description

**Jupii** let you play audio, video and image files on any device on your local network that supports UPnP/DLNA, such as smart speaker, smart TVs, gaming consoles, and more.

In addition to the typical features you might expect from this type of application, **Jupii** also has some unique functionalities such as:

- support many different internet services as media source (Bandcamp, SoundCloud, YouTube and many more)
- live casting of video/audio from camera or microphone
- screen mirroring (only on X11)
- recorder that let you to extract music from internet radio streams

This app can be used in two different UPnP/DLNA modes:

- **Playback Control mode**: Using Jupii, you connect to the player device (e.g. smart speaker) and transfer media from your phone/computer to this device.
- **Media Server mode**: Using your playback device (e.g. smart TV), you browse and play media files shared by Jupii.

## How to install

- Linux Desktop: [Flatpak](https://flathub.org/apps/net.mkiol.Jupii)
- Sailfish OS: [OpenRepos](https://openrepos.net/content/mkiol/jupii)

## Building from sources

### Flatpak

```
git clone <git repository url>

cd Jupii/flatpak

flatpak-builder --user --install-deps-from=flathub --repo="/path/to/local/flatpak/repo" "/path/to/output/dir" net.mkiol.Jupii.yaml
```

### Sailfish OS

```
git clone <git repository url>

cd Jupii
mkdir build
cd build

sfdk config --session specfile=../sfos/harbour-jupii.spec
sfdk config --session target=SailfishOS-4.4.0.58-aarch64
sfdk cmake ../ -DCMAKE_BUILD_TYPE=Release -DWITH_SFOS=ON
sfdk package
```

### Linux (direct build)

```
git clone <git repository url> jupii

cd jupii
mkdir build
cd build

cmake ../ -DCMAKE_BUILD_TYPE=Release -DWITH_DESKTOP=ON
make
```

## Contributing to Jupii

Any contribution is very welcome!

Project is hosted both on [GitHub](https://github.com/mkiol/jupii) and [GitLab](https://gitlab.com/mkiol/jupii).
Feel free to make a PR/MR, report an issue or reqest for new feature on the platform you prefer the most.

### Translations

Translation files in Qt format are in `translations` directory.

Preferred way to contribute translation is via [Transifex service](https://www.transifex.com/mkiol/jupii),
but if you would like to make a direct PR, please do it.

## How to support

If you find **Jupii** useful and would like to support this project,
please consider doing one or two of the following:

- Give a &#11088; on [GitHub](https://github.com/mkiol/jupii) or/and [GitLab](https://gitlab.com/mkiol/jupii).
- Write a review in your applications manager app (Discover, Software or any other).
- Tell others about this app by mentioning it on social media.
- If you have spare money, make a small donation via [Liberapay](https://liberapay.com/mkiol/donate).

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

