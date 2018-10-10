# Jupii
The UPnP/DLNA client for Sailfish OS and Linux desktop. It allows to stream content files (Music, Video, Images) to UPnP/DLNA devices.

## Features
Following features are currently supported:
- Discovery of UPnP devices in a local network
- Remote control (Play, Pause, Next, Prev, Seek, Volume up/down)
- Streaming of local content (Music, Video, Images) as well as remote content (e.g. Internet radio channels) to UPnP Media Renderer devices

## Role in UPnP architecture
Jupii is a client of AVTransport and RenderingControl services. It connects to devices that implement MediaRenderer role. In order to share content, Jupii starts local HTTP streaming server.

## D-Bus API
Jupii exposes simple [D-Bus service](https://github.com/mkiol/Jupii/blob/master/dbus/org.jupii.xml). It can be used to make integration with other applications.

The example 'proof of concept' [integration with gPodder on Sailfish OS](https://github.com/mkiol/Jupii/raw/master/screenshots/jupii-sailfish-gpodder.png) is available to download [here](https://github.com/mkiol/Jupii/raw/master/binary/harbour-org.gpodder.sailfish-4.6.0-1.noarch-jupii.rpm).

## Third-party components
Jupii relies on following third-party open source components:
* [QHTTPServer](https://github.com/nikhilm/qhttpserver) by Nikhil Marathe
* [Libupnpp](https://opensourceprojects.eu/p/libupnpp) by J.F.Dockes
* [Libupnp](http://upnp.sourceforge.net) by Intel Corporation and open source community
* [TagLib](http://taglib.org/) by TagLib community

## Download
* Sailfish OS packages are available for download from [OpenRepos](https://openrepos.net/content/mkiol/jupii) and Jolla Store.
* Linux desktop binary packages (RPM, DEB and package for Arch) can be download [here](https://github.com/mkiol/Jupii/tree/master/desktop/packages).

## License
Jupii is distributed under
[Mozilla Public License Version 2.0](https://www.mozilla.org/MPL/2.0/).
