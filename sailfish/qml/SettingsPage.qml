/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: root

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Settings")
            }

            TextSwitch {
                automaticCheck: false
                checked: settings.rememberPlaylist
                text: qsTr("Start with last playlist")
                description: qsTr("When Jupii starts, the last " +
                                  "playlist will be automatically loaded.")
                onClicked: {
                    settings.rememberPlaylist = !settings.rememberPlaylist
                }
            }

            Slider {
                width: parent.width
                minimumValue: 1
                maximumValue: 60
                stepSize: 1
                handleVisible: true
                value: settings.forwardTime
                valueText: value + " s"
                label: qsTr("Forward/backward time-step interval")

                onValueChanged: {
                    settings.forwardTime = value
                }
            }

            SectionHeader {
                text: qsTr("Experiments")
            }

            TextSwitch {
                automaticCheck: false
                checked: settings.useDbusVolume
                text: qsTr("Volume control with hardware keys")
                description: qsTr("Change volume level using phone hardware volume keys. " +
                                  "The volume level of the media device will be " +
                                  "set to be the same as the volume level of the ringing alert " +
                                  "on the phone.")
                onClicked: {
                    settings.useDbusVolume = !settings.useDbusVolume
                }
            }

            TextSwitch {
                automaticCheck: false
                checked: settings.imageSupported
                text: qsTr("Image content")
                description: qsTr("Playing images on UPnP devices doesn't work well right now. " +
                                  "There are few minor issues that have not been resolved yet. " +
                                  "This option forces %1 to play images despite the fact " +
                                  "it could cause some issues.").arg(APP_NAME)
                onClicked: {
                    settings.imageSupported = !settings.imageSupported
                }
            }

            /*TextSwitch {
                automaticCheck: false
                checked: settings.pulseSupported
                text: qsTr("Capture audio output (restart needed)")
                description: qsTr("This option enables capturing the audio output of any application. " +
                                  "It provides similar functionality to pulseaudio-dlna server. It means that " +
                                  "%1 can stream certain application's PulseAudio playback to an UPnP/DLNA device. " +
                                  "For instance, you can capture web browser audio playback to listen YouTube on a remote speaker. " +
                                  "When enabled, \"Audio output\" option is visible in \"Add item\" menu. " +
                                  "%1 restart is needed to apply changes.").arg(APP_NAME)
                onClicked: {
                    settings.pulseSupported = !settings.pulseSupported
                }
            }*/

            ComboBox {
                // modes:
                // 0 - MP3 16-bit 44100 Hz stereo 128 kbps (default)
                // 1 - MP3 16-bit 44100 Hz stereo 96 kbps
                // 2 - LPCM 16-bit 44100 Hz stereo 1411 kbps
                // 3 - LPCM 16-bit 22050 Hz stereo 706 kbps
                label: qsTr("Audio capture format")
                description: qsTr("Stream format used when %1 captures the audio output of other application. " +
                                  "Uncompressed stream (PCM) results in lower delay but " +
                                  "the higher bitrate likely will cause quicker battery drain.").arg(APP_NAME)
                currentIndex: settings.pulseMode
                menu: ContextMenu {
                    MenuItem { text: qsTr("MP3 44100Hz 128 kbps (default)") }
                    MenuItem { text: qsTr("MP3 44100Hz 96 kbps") }
                    MenuItem { text: qsTr("PCM 44100Hz 1411 kbps") }
                    MenuItem { text: qsTr("PCM 22050Hz 706 kbps") }
                }

                onCurrentIndexChanged: {
                    settings.pulseMode = currentIndex
                }
            }

            TextSwitch {
                automaticCheck: false
                checked: settings.showAllDevices
                text: qsTr("All devices visible")
                description: qsTr("%1 supports only Media Renderer devices. With this option enabled, " +
                                  "all UPnP devices will be shown, including unsupported devices like " +
                                  "home routers or Media Servers. For unsupported devices %1 is able " +
                                  "to show only basic description information. " +
                                  "This option could be useful for auditing UPnP devices " +
                                  "in your local network.").arg(APP_NAME)
                onClicked: {
                    settings.showAllDevices = !settings.showAllDevices
                }
            }

            TextSwitch {
                automaticCheck: false
                checked: settings.ssdpIpEnabled
                text: qsTr("Adding devices manually")
                description: qsTr("If %1 fails to discover a device " +
                             "(e.g. because it is in a different LAN), you can " +
                             "add it manually with IP address. " +
                             "When enabled, pull down menu contains additional " +
                             "option to add device manually. " +
                             "Make sure that your device is not behind a NAT " +
                             "or a firewall.").arg(APP_NAME)
                onClicked: {
                    settings.ssdpIpEnabled = !settings.ssdpIpEnabled
                }
            }

            ComboBox {
                label: qsTr("Internet streaming mode")
                description: qsTr("Streaming from the Internet to UPnP devices can be handled in two modes: Proxy (default) or Redirection. In Proxy mode, %1 relays every packet received from a streaming host (e.g. internet radio server) to a UPnP device located in your home network. This mode is transparent for a UPnP device, so it works in most cases. Because packets goes through your phone/tablet, %1 must be enabled all the time to make a streaming working. In Redirection mode, %1 uses HTTP redirection to instruct UPnP device where internet host is located. The actual streaming goes directly between UPnP device and a streaming server, so %1 in not required to be enabled all the time. The downside of Redirection mode is that not every UPnP device supports redirection. Therefore on some devices this mode will not work properly.").arg(APP_NAME)
                currentIndex: settings.remoteContentMode
                menu: ContextMenu {
                    MenuItem { text: qsTr("Proxy") }
                    MenuItem { text: qsTr("Redirection") }
                }

                onCurrentIndexChanged: {
                    settings.remoteContentMode = currentIndex
                }
            }

            Spacer {}
        }
    }

    VerticalScrollDecorator {
        flickable: flick
    }
}
