# Jupii
The UPnP/DLNA client for Sailfish OS. It allows to stream content files from the mobile to UPnP/DLNA devices.

## Features
Following features are currently supported:
- Discovery of UPnP devices in a local network
- Remote control (Play, Pause, Next, Prev, Seek, Volume up/down)
- Streaming of content (Music, Video, Images) from the mobile to UPnP devices

## Role in UPnP architecture
Jupii is a client of AVTransport and RenderingControl services. It connects to devices that implement MediaRenderer role. In order to share content from the mobile, Jupii starts local HTTP streaming server.

## Third-party components
Jupii relies on following third-party open source components:
* [QHTTPServer](https://github.com/nikhilm/qhttpserver) by Nikhil Marathe
* [Libupnpp](https://opensourceprojects.eu/p/libupnpp) by J.F.Dockes
* [Libupnp](http://upnp.sourceforge.net) by Intel Corporation and open source community
* [TagLib](http://taglib.org/) by TagLib community

## License
Jupii is distributed under
[Mozilla Public License Version 2.0](https://www.mozilla.org/MPL/2.0/).
