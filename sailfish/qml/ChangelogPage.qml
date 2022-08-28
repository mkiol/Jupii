/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.height

        Column {
            id: content

            width: root.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Changes")
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.13")
            }

            LogItem {
                title: "YouTube Music browser"
                description: "YouTube Music is a popular video and music streaming platform. " +
                             "You can search and add content you like using a browser available via 'Add Items' menu. " +
                             "To switch between audio/video use 'Type' option in pull-down menu. " +
                             "YouTube Music integration is possible thanks to yt-dlp, ytmusicapi and KDE's AudioTube app. " +
                             "Known issue: Searching and adding items is quite slow right now."
            }

            LogItem {
                title: "Caching"
                description: "For better performance, remote content can be fully downloaded before playing. " +
                             "If an item is cached any re-plays don't require downloading content from the Internet. " +
                             "Moreover, with caching, transcoding and audio extracting for URL items is possible."
            }

            LogItem {
                title: "Audio extracting from remote video"
                description: "Option 'Add URL' has a new switch 'Add only audio stream'. " +
                             "When it's enabled, instead of video, audio stream is extracted and played on UPnP device."
            }

            LogItem {
                title: "Media type indicator"
                description: "Small UI improvement. An item on play queue has small icon which indicates whether " +
                             "content type is Video, Audio or Image."
            }

            LogItem {
                title: "Permission for integration with gPodder app"
                description: "Integration with gPodder has not worked since sandboxing was enabled. " +
                             "To overcome this problem, Jupii requests for 'gPodder' permission " +
                             "when it's run for the first time."
            }

            LogItem {
                title: "Screen capture only without sandboxing"
                description: "Screen capture does not work right now under sandboxing. " +
                             "This experimental feature is available only when app is running without SailJail. " +
                             "To enable Screen capture you have to manually add 'Sandboxing=Disabled' to harbour-jupii.desktop file."
            }

            LogItem {
                title: "Improved Bandcamp and SoundCloud browsers"
                description: "All featured items for Bandcamp and SoundCloud are now available for browsing. " +
                             "To load more featured tracks or albums, click on 'Show more' button at the bottom of the items list. " +
                             "Moreover, list item contains label with genre name."
            }

            LogItem {
                title: "Bug fixes and improvements"
                description: "Many small and big bugs have been fixed. " +
                             "For example, problem with playing very large video files is resolved."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.12")
            }

            LogItem {
                title: "Explicit app permissions"
                description: "Jupii requests for following permissions: Internet, Audio, Microphone, RemovableMedia, UserDirs, MediaIndexing."
            }

            LogItem {
                title: "Screen capture disabled"
                description: "Due to sandboxing, 'Screen capture' feature was broken and is therefore disabled from now on."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.11")
            }

            LogItem {
                title: "FOSDEM 2022 browser"
                description: "FOSDEM is a conference centered on free and open-source " +
                             "software development. It is well known for having excellent video recordings. " +
                             "Now you can browse and add videos from FOSDEM 2022."
            }

            LogItem {
                title: "Fixes for Bandcamp browser"
                description: "Bandcamp API was changed and therefore adding music from Bandcamp browser was broken. " +
                             "This issue is now resolved."
            }

            LogItem {
                title: "Recent searches"
                description: "Bandcamp, Icecast, SoundCloud and TuneIn browsers display the most recent searched phrases."
            }

            LogItem {
                title: "Featured content"
                description: "Bandcamp and SoundCloud browsers have an option to show 'New and Notable' albums (Bandcamp) and 'Trending tracks' (SoundCloud)."
            }

            LogItem {
                title: "Remote content playback improvements"
                description: "Before playback, URL-based content from the Internet is cached to improve streaming stability. " +
                             "This might resolve some issues observed on TV devices (e.g. Samsung TV)."
            }

            LogItem {
                title: "Album and artist URLs detection in 'Open link'"
                description: "When you open URL pointing to Bandcamp/SoundCloud's album or artist web page, browser page will be shown."
            }

            LogItem {
                title: "Visual improvements"
                description: "Few UI glitches were fixed. Landscape view of Player panel looks better now."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.10")
            }

            LogItem {
                title: "Fixes for bugs discovered in SFOS 4.3"
                description: "Due to Tracker upgrade in SFOS 4.3, Album and Artist browser did not work correctly. " +
                             "Hopefully this issue is now resolved."
            }

            LogItem {
                title: "Fixes for SoundCloud integration"
                description: "SoundCould API has been changed and therefore adding music from SoundCloud browser was broken. " +
                             "This issue is now resolved."
            }

            LogItem {
                title: "Jupii as an option in 'Open link' dialog"
                description: "Jupii can open links. It now appears in 'Open link' dialog when you click on the link in other application. " +
                             "If you don't like this feature, you can disable it in the Settings menu."
            }

            LogItem {
                title: "External libraries update"
                description: "FFmpeg, Libupnpp and x264 have been updated to the most recent versions."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.9")
            }

            LogItem {
                title: "SoundCloud browser"
                description: "SoundCloud is an online music community and sharing website. " +
                             "You can search and add audio content from SoundCloud using simple browser. " +
                             "New option is available via 'Add Items' menu."
            }

            LogItem {
                title: "FOSDEM 2021 browser"
                description: "FOSDEM is a conference centered on free and open-source " +
                             "software development. It is well known for having excellent video recordings. " +
                             "Now you can browse and add videos from FOSDEM 2021."
            }

            LogItem {
                title: "Frontier Silicon API"
                description: "Frontier Silicon chipset is used in some digital radios products from " +
                             "Hama, Medion, Slivercrest, Auna, Technisat, Revo, Pinnel and more. " +
                             "Initial support for the FSAPI has been added. " +
                             "When remote device supports FSAPI, option to toggle power is visible in the context menu."
            }

            LogItem {
                title: "Performance improvements and fixes for bugs discovered on SFOS 4.0"
                description: "Jupii starts quicker and uses less memory. " +
                             "Minor bugs discovered on SFOS 4.0 have been fixed as well."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.8")
            }

            LogItem {
                title: "TuneIn browser"
                description: "TuneIn is a popular online radio directory. " +
                             "It enables discovery of internet radio stations " +
                             "from all over the world. " +
                             "Right now Jupii is able to play only HTTP stations. " +
                             "ICY or MMS streamings are not supported therefore " +
                             "some TuneIn stations cannot be added."
            }

            LogItem {
                title: "Better Bandcamp browser"
                description: "Bandcamp is a music streaming online service. " +
                             "Previously the browser used Youtube-dl " +
                             "to discover media URLs, but now other approach is chosen. " +
                             "It is much faster and enables also browsing by Albums and Artists."
            }

            LogItem {
                title: "Tips and hints for the fist time user"
                description: "Fist time experience is improved. " +
                             "Tips, swipe and touch hints are shown to navigate user " +
                             "through the app."
            }

            LogItem {
                title: "Fixes for bugs discovered on SFOS 3.4.0"
                description: "In the recent SFOS version Albums, Artists and " +
                             "Playlists browser stopped working. " +
                             "This issue is now resolved."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.7")
            }

            LogItem {
                title: "FOSDEM browser"
                description: "FOSDEM is a conference centered on free and open-source " +
                             "software development. It is well known for having excellent video recordings. " +
                             "You can browse and add video content from FOSDEM conferences via 'Add Items' menu."
            }

            LogItem {
                title: "URL resolution with Youtube-dl"
                description: "If URL doesn't directly point to a media content, " +
                             "Youtube-dl will be used to find a proper media URL. " +
                             "Thank to this option, you can add " +
                             "URLs to popular websites and Jupii will try to extract media content from them. " +
                             "Some URLs discovered with Youtube-dl have expiration time. " +
                             "To check and update Youtube-dl URLs that are in the play queue use " +
                             "'Refresh items' menu option."
            }

            LogItem {
                title: "Bandcamp browser"
                description: "Bandcamp is a music streaming online service. " +
                             "You can search and add audio content from Bandcamp using simple browser. " +
                             "Bandcamp browser uses Youtube-dl to discover media URLs. " +
                             "New option is available via 'Add Items' menu."
            }

            LogItem {
                title: "Stream recorder enhancement (Experiment)"
                description: "Recorder is now able to record all kinds of audio streams. " +
                             "It can be use to save audio file from any URL item. " +
                             "Following audio formats are supported: mp3, mp4, ogg, aiff, wav."
            }

            LogItem {
                title: "Option to set the quality of Screen capture (Experiment)"
                description: "Due to software-based encoding, Screen capturing " +
                             "is extremely resource hungry and therefore may provide " +
                             "very unstable video stream. " +
                             "New option allows you to change the quality of video to better tune " +
                             "final user experience."
            }

            LogItem {
                title: "Audio capture improvement"
                description: "In the recent SFOS version Audio capture worked very poorly. " +
                             "Hopefully this issue is now resolved and Audio capture can be used to " +
                             "stream audio playback of any application without a problem."
            }

            LogItem {
                title: "Spanish, Swedish, Slovenian, German, Russian and Chinese translations update"
                description: "Many thanks to Carlos Gonzalez, Åke Engelbrektson, " +
                             "Boštjan Štrumbelj, drosjesjaafoe, Вячеслав Диконов " +
                             "and Rui Kon for providing updated translations."
            }

            LogItem {
                title: "DBus API update"
                description: "DBus API has been updated to provide better " +
                             "integration with other apps (e.g. Microtube)."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.6")
            }

            LogItem {
                title: "Items from UPnP Media Servers on the play queue"
                description: "Support for content located on UPnP Media Servers is added. " +
                             "You can browse and add new type of items by using 'Add Items' menu."
            }

            LogItem {
                title: "Favorite devices option"
                description: "Devices discovery takes few seconds. If you mark " +
                             "a device as favorite, it will be treated as always available " +
                             "and therefore you will be able to connect without wait."
            }

            LogItem {
                title: "Play queue usability enhancement"
                description: "Navigation on the play queue is improved. You can access " +
                             "play queue without being connected to any device. "
            }

            LogItem {
                title: "Tracks history for Icecast items"
                description: "The history of played tracks in Icecast stream is " +
                             "visible on media info page"
            }

            LogItem {
                title: "Force 16:9 aspect ratio option for Screen capture"
                description: "When screen aspect ratio is not 16:9 (e.g. Xperia 10), " +
                             "you can crop/scale screen to 16:9."
            }

            LogItem {
                title: "Bug fixes and many small improvements"
                description: "Issues releated to volume control with HW keys, " +
                             "sorting on devices list, WLAN on/off handling, " +
                             "occasional crashes and many more are resolved."
            }

            LogItem {
                title: "Option to turn on logging to file"
                description: "Needed for diagnostic purposes only. " +
                             "When enabled log data is stored in %1/jupii.log file."
                .arg(settings.getCacheDir())
            }

            LogItem {
                title: "Chinese, German, Russian, Spanish, Slovenian and Swedish translations update"
                description: "Many thanks to Rui Kon, drosjesjaafoer, jgibbon, Petr Tsymbarovich, " +
                             "Carlos Gonzalez, Bostjan Strumbelj and Åke Engelbrektson " +
                             "for providing updated translations."
            }

            /*SectionHeader {
                text: qsTr("Version %1").arg("2.5")
            }

            LogItem {
                title: "Sharing playlist items via UPnP"
                description: "Items on current playlist will be accessible for " +
                             "other UPnP devices through content directory service. " +
                             "When this option is enabled, Jupii is visible in a UPnP local network."
            }

            LogItem {
                title: "Screen capture (Experiment)"
                description: "Screen casting feature. It provides stable but low framerate streaming. " +
                             "When audio capturing is disabled (default) video should reach 5 fps framerate. " +
                             "Otherwise framerate is lower. " +
                             "Be aware that due to buffering there is a delay " +
                             "between phone and the video displayed on the remote screen. " +
                             "The implementation is inspired and partially based on " +
                             "<a href=\"https://github.com/mer-qa/lipstick2vnc/\">lipstick2vnc</a> project."
            }

            LogItem {
                title: "Stream recorder (Experiment)"
                description: "Enables recording of tracks from Icecast stream. " +
                             "When stream provides information " +
                             "about the title of the currently played track, " +
                             "you can save this track to a file. " +
                             "Stream recording feature can be turn on in Experiments " +
                             "section in the settings."
            }

            LogItem {
                title: "Microphone and Audio capture improvements"
                description: "Audio capturing and Microphone features have been significantly improved. " +
                             "Audio streaming has lower delay and the overall performance, " +
                             "including impact on battery, is much better. " +
                             "Moreover, microphone data is now always encoded to MP3 which provides " +
                             "better compatibility with various DLNA devices."
            }

            LogItem {
                title: "Landscape mode"
                description: "Landscape orientation is now supported."
            }

            LogItem {
                title: "Better volume up/down hardware keys support"
                description: "You can change the volume level of a remote DLNA " +
                             "device using phone hardware " +
                             "keys without changing phone volume level. " +
                             "Support for hardware keys is enabled by default but it can be " +
                             "disabled in the settings"
            }

            LogItem {
                title: "Improved search on Album, Recordings and gPodder pages"
                description: "Searching takes into account many different fields like: " +
                             "album title, artist name, station name, podcast and episode title. " +
                             "There is also an option to sort search results."
            }

            LogItem {
                title: "SomaFM streaming improvements"
                description: "Streaming of SomaFM has been improved to be more " +
                             "compatible with various DLNA implementations. " +
                             "Especially it applies to Samsung TV devices. "
            }

            LogItem {
                title: "Initial Yamaha XC API support - Toggle power option"
                description: "When remote device supports Yamaha XC API, " +
                             "option to toggle power is visible in the context menu."
            }

            LogItem {
                title: "DBus API improvements"
                description: "App is bring to foreground when addPathOnceAndPlay " +
                             "or addUrlOnceAndPlay is invoked."
            }

            LogItem {
                title: "Italian translation"
                description: "Many thanks to Fra for " +
                             "providing Italian translation."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.2")
            }

            LogItem {
                title: "Audio capture of any application"
                description: "This option enables capturing audio output of any application. " +
                             "It provides similar functionality to pulseaudio-dlna server. It means that " +
                             "Jupii can stream certain application's audio playback to a DLNA device. " +
                             "For instance, you can capture web browser audio playback to listen YouTube on a remote speaker. " +
                             "You can enable audio capturing by adding \"Audio capture\" from \"Add item\" menu. " +
                             "By default captured audio stream will be encoded to MP3 format. " +
                             "Encoding adds extra delay comparing to uncommpressed stream but it is " +
                             "much more efficient for overall performance. " +
                             "A stream format/quality can be changed in the settings (Experiments section). "
            }

            LogItem {
                title: "Option to update SomaFM channel list"
                description: "To download the latest list of SomaFM channels " +
                             "choose \"Refresh channel list\" from pull-down menu. " +
                             "If you enjoy SomaFM radio, please consider making a " +
                             "<a href=\"http://somafm.com/support/\">donation</a>."
            }

            LogItem {
                title: "Initial support for HLS URLs"
                description: "Very basic support for HTTP Live Streaming has been added. " +
                             "When Jupii detects that URL points to HLS playlist it will " +
                             "just pass it to a DLNA device without streaming relay. " +
                             "Your DLNA device has to support HLS to make it work."
            }

            LogItem {
                title: "Streaming improvements"
                description: "Streaming of local files has been improved to be more compatible with various DLNA implementations. " +
                             "Especially it applies to Samsung TV devices. "
            }

            LogItem {
                title: "Better support for playlist formats"
                description: "Playlists with relative URLs are now accepted."
            }

            LogItem {
                title: "DBus API extension"
                description: "Addional methods to DBus API have been added."
            }

            LogItem {
                title: "Slovenian and Chinese translations"
                description: "Many thanks to Rui Kon and Boštjan Štrumbelj for " +
                             "providing Chinese and Slovenian translations."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.0.0")
            }

            LogItem {
                title: "Support for Internet streaming content"
                description: "In addition to local files, URL content " +
                             "(e.g. internet radio streams, remote media files) can be added to the playlist queue. " +
                             "An URL should point to direct stream or to a playlist file. " +
                             "Only HTTP URLs are supported right now."
            }

            LogItem {
                title: "SomaFM channels"
                description: "As a playlist item, you can add SomaFM radio channel. " +
                             "SomaFM is an independent Internet-only streaming service, " +
                             "supported entirely with donations from listeners. " +
                             "If you enjoy SomaFM radio, please consider making a " +
                             "<a href=\"http://somafm.com/support/\">donation</a>."
            }

            LogItem {
                title: "Icecast directory"
                description: "Icecast directory is a listing of broadcast streams. " +
                             "Jupii has built-in browser for searching and adding " +
                             "Icecast stations to its playlist queue."
            }

            LogItem {
                title: "Podcasts browser for gPodder"
                description: "A convenient built-in browser that allows you to add episodes " +
                             "previously downloaded with gPodder podcast player. " +
                             "Option is visible only if gPodder app is installed."
            }

            LogItem {
                title: "Microphone"
                description: "Use microphone as a source for audio stream that is played on your DLNA device. " +
                             "Add Item list contains additional item - Microphone."
            }

            LogItem {
                title: "Playlist queue polish"
                description: "Playlist queue has been polished. When track provides meta data, " +
                             "playlist item contains title, author and album art image."
            }

            LogItem {
                title: "Improved support for playlist files"
                description: "Interface for browsing and adding tracks from playlist files has been improved. " +
                             "Now it is possible to search and select individual tracks. " +
                             "Additionally, the file picker supports three major playlist formats (M3U, XSPF and PLS)."
            }

            LogItem {
                title: "Icecast meta data support"
                description: "Many internet radio services use Icecast streaming. " +
                             "Jupii is able to display stream title retrieved from meta data. " +
                             "It also removes in-stream meta data if DLNA device doesn't support Icecast protocol."
            }

            LogItem {
                title: "Improved cover design"
                description: "Cover page has a new background image."
            }

            LogItem {
                title: "General performance and stability improvements"
                description: "Overall stability and performance have been improved."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("1.0.0")
            }

            LogItem {
                title: "Spanish Swedish & German translations"
                description: "Many thanks to Carlos Gonzalez for " +
                             "providing Spanish translation."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("0.9.6")
            }

            LogItem {
                title: "Swedish & German translations"
                description: "Many thanks to Åke Engelbrektson and drosjesjaafoer for " +
                             "providing Swedish and German translations."
            }

            LogItem {
                title: "Improved stability and bug fixes"
                description: "Resilience to crash failures and other errors " +
                             "has been significantly improved."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("0.9.5")
            }

            LogItem {
                title: "Better support for various UPnP devices"
                description: "Support for diffrent UPnP devices has been improved."
            }

            LogItem {
                title: "Russian, Dutch & Polish translations"
                description: "Many thanks to Вячеслав Диконов and d9h20f for " +
                             "providing Russian and Dutch translations."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("0.9.4")
            }

            LogItem {
                title: "Play audio stream extracted from video files"
                description: "Some DLNA devices support only audio content. " +
                             "In order to play video files on such devices, " +
                             "Jupii can extract an audio stream from a video file."
            }

            LogItem {
                title: "Music picker sorted by artists"
                description: "The music tracks can be browsed by artist name."
            }

            LogItem {
                title: "Playlist file picker"
                description: "The music tracks from a playlist file can be " +
                             "added to the current playlist."
            }

            LogItem {
                title: "Save current playlist to a file"
                description: "Current playlist can be saved as a playlist file."
            }

            LogItem {
                title: "Add device manually (experimental)"
                description: "If Jupii fails to discover a device " +
                             "(e.g. because it is in a different LAN), you can " +
                             "add it manually with IP address. Make sure " +
                             "that device is not behind a NAT or a firewall."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("0.9.3")
            }

            LogItem {
                title: "Music album picker"
                description: "The music tracks can be browsed by album name. " +
                             "The whole album or individual tracks can be " +
                             "added to the current playlist."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("0.9.2")
            }

            LogItem {
                title: "Repeat play mode"
                description: "Items on the playlist can be playbacked in " +
                             "Normal, Repeat-All or Repeat-One mode."
            }

            LogItem {
                title: "Multi-item pickers"
                description: "Music, Video, Image or File pickers support " +
                             "selection of multiple items."
            }

            LogItem {
                title: "Start with last playlist"
                description: "When Jupii connects to a device, the last saved " +
                             "playlist will be automatically loaded. " +
                             "If you don't like this feature it can be disabled " +
                             "in the settings."
            }

            LogItem {
                title: "Volume control with hardware keys"
                description: "Change volume level using phone hardware volume keys. " +
                             "The volume level of the media device will be " +
                             "set to be the same as the volume level of the ringing alert " +
                             "on the phone. Option can be disabled in the settings."
            }

            LogItem {
                title: "D-Bus API"
                description: "Jupii exposes simple D-Bus service. It can be use " +
                             "to make integration with other Sailfish OS applications. " +
                             "The example 'proof of concept' intergation with gPodder " +
                             "is available to download from Jupii GitHub page."
            }

            LogItem {
                title: "Improvements of the player UI"
                description: "Player bottom panel has more compact look. " +
                             "If needed, it can be also expanded to the full size."
            }

            SectionHeader {
                text: qsTr("Version %1").arg("0.9.1")
            }

            LogItem {
                title: "Initial release"
                description: "This is initial public release.";
            }*/

            Spacer {}
        }
    }

    VerticalScrollDecorator {
        flickable: flick
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
