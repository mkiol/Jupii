/* Copyright (C) 2017-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.Settings 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

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

            InlineMessage {
                visible: settings.restartRequired
                width: parent.width
                text: qsTr("Restart is required for the changes to take effect.")
            }

            TextSwitch {
                automaticCheck: false
                checked: settings.contentDirSupported
                text: qsTr("Share play queue items via UPnP Media Server")
                description: qsTr("When enabled, items in play queue are accessible " +
                                  "for other UPnP devices in your local network.")
                onClicked: {
                    settings.contentDirSupported = !settings.contentDirSupported
                }
            }

            ExpandingSectionGroup {
                ExpandingSection {
                    title: qsTr("Hardware keys")

                    content.sourceComponent: Column {

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.useHWVolume
                            text: qsTr("Volume control with hardware keys")
                            onClicked: {
                                settings.useHWVolume = !settings.useHWVolume
                            }
                        }

                        Slider {
                            enabled: settings.useHWVolume
                            opacity: enabled ? 1.0 : Theme.opacityLow
                            width: parent.width
                            minimumValue: 1
                            maximumValue: 10
                            stepSize: 1
                            handleVisible: true
                            value: settings.volStep
                            valueText: value
                            label: qsTr("Volume level step")

                            onValueChanged: {
                                settings.volStep = value
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Streaming format")

                    content.sourceComponent: Column {
                        ComboBox {
                            label: qsTr("Video streaming format")
                            currentIndex: {
                                switch (settings.casterVideoStreamFormat) {
                                case Settings.CasterStreamFormat_MpegTs: return 0;
                                case Settings.CasterStreamFormat_Mp4: return 1;
                                }
                                return 0;
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("MPEG-TS") }
                                MenuItem { text: qsTr("MP4") }
                            }

                            onCurrentIndexChanged: {
                                switch (currentIndex) {
                                case 0: settings.casterVideoStreamFormat = Settings.CasterStreamFormat_MpegTs; break;
                                case 1: settings.casterVideoStreamFormat = Settings.CasterStreamFormat_Mp4; break;
                                default: settings.casterVideoStreamFormat = Settings.CasterStreamFormat_Mp4;
                                }
                            }
                            description: qsTr("Change if you observe problems with video playback in Camera or Screen capture.")
                        }

                        ComboBox {
                            label: qsTr("Audio streaming format")
                            currentIndex: {
                                switch (settings.casterAudioStreamFormat) {
                                case Settings.CasterStreamFormat_Mp3: return 0;
                                case Settings.CasterStreamFormat_Mp4: return 1;
                                case Settings.CasterStreamFormat_MpegTs: return 2;
                                }
                                return 0;
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("MP3") }
                                MenuItem { text: qsTr("MP4") }
                                MenuItem { text: qsTr("MPEG-TS") }
                            }

                            onCurrentIndexChanged: {
                                switch (currentIndex) {
                                case 0: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp3; break;
                                case 1: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp4; break;
                                case 2: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_MpegTs; break;
                                default: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp3;
                                }
                            }

                            description: qsTr("Change if you observe problems with audio playback in Microphone or Audio capture.")
                        }
                    }
                }
                ExpandingSection {
                    title: qsTr("Recorder")

                    content.sourceComponent: Column {
                        ListItem {
                            contentHeight: visible ? recflow.height + 2*Theme.paddingLarge : 0

                            Flow {
                                id: recflow
                                anchors {
                                    verticalCenter: parent.verticalCenter
                                    left: parent.left; right: parent.right
                                    leftMargin: Theme.paddingLarge
                                    rightMargin: Theme.paddingLarge
                                }
                                spacing: Theme.paddingMedium

                                Label {
                                    text: qsTr("Directory for recordings")
                                }

                                Label {
                                    color: Theme.highlightColor
                                    text: utils.dirNameFromPath(settings.recDir)
                                }
                            }

                            onClicked: openMenu();

                            menu: ContextMenu {
                                MenuItem {
                                    text: qsTr("Change")
                                    onClicked: {
                                        var obj = pageStack.push(Qt.resolvedUrl("DirPage.qml"));
                                        obj.accepted.connect(function() {
                                            settings.recDir = obj.selectedPath
                                        })
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Set default")
                                    onClicked: {
                                        settings.recDir = ""
                                    }
                                }
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Caching")

                    content.sourceComponent: Column {
                        ComboBox {
                            label: qsTr("Cache remote content")
                            currentIndex: {
                                if (settings.cacheType === Settings.Cache_Auto)
                                    return 0
                                if (settings.cacheType === Settings.Cache_Always)
                                    return 1
                                if (settings.cacheType === Settings.Cache_Never)
                                    return 2
                                return 0
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("Auto") }
                                MenuItem { text: qsTr("Always") }
                                MenuItem { text: qsTr("Never") }
                            }

                            onCurrentIndexChanged: {
                                if (currentIndex == 0)
                                    settings.cacheType = Settings.Cache_Auto
                                else if (currentIndex == 1)
                                    settings.cacheType = Settings.Cache_Always
                                else if (currentIndex == 2)
                                    settings.cacheType = Settings.Cache_Never
                                else
                                    settings.cacheType = Settings.Cache_Auto
                            }
                        }

                        ComboBox {
                            label: qsTr("Cache cleaning")

                            currentIndex: {
                                if (settings.cacheCleaningType === Settings.CacheCleaning_Auto)
                                    return 0
                                if (settings.cacheCleaningType === Settings.CacheCleaning_Always)
                                    return 1
                                if (settings.cacheCleaningType === Settings.CacheCleaning_Never)
                                    return 2
                                return 0
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("Auto") }
                                MenuItem { text: qsTr("Always") }
                                MenuItem { text: qsTr("Never") }
                            }

                            onCurrentIndexChanged: {
                                if (currentIndex == 0)
                                    settings.cacheCleaningType = Settings.CacheCleaning_Auto
                                else if (currentIndex == 1)
                                    settings.cacheCleaningType = Settings.CacheCleaning_Always
                                else if (currentIndex == 2)
                                    settings.cacheCleaningType = Settings.CacheCleaning_Never
                                else
                                    settings.cacheCleaningType = Settings.CacheCleaning_Auto
                            }
                        }

                        ListItem {
                            contentHeight: flow0.height + 2 * Theme.paddingLarge
                            visible: settings.isDebug()
                            onClicked: openMenu()

                            Flow {
                                id: flow0
                                spacing: Theme.paddingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.leftMargin: Theme.paddingLarge
                                anchors.rightMargin: Theme.paddingLarge

                                Label {
                                    text: qsTr("Cache size")
                                }

                                Label {
                                    id: sizeLabel
                                    color: Theme.secondaryColor
                                    text: cserver.cacheSizeString()

                                    Connections {
                                        target: cserver
                                        onCacheSizeChanged: sizeLabel.text = cserver.cacheSizeString()
                                    }
                                }
                            }

                            menu: ContextMenu {
                                MenuItem {
                                    text: qsTr("Delete cache")
                                    onClicked: cserver.cleanCache()
                                }
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Advanced")
                    content.sourceComponent: Column {
                        ComboBox {
                            label: qsTr("Preferred network interface")
                            currentIndex: utils.prefNetworkInfIndex()
                            menu: ContextMenu {
                                Repeater {
                                    model: utils.networkInfs()
                                    MenuItem { text: modelData }
                                }
                            }
                            onCurrentIndexChanged: {
                                utils.setPrefNetworkInfIndex(currentIndex)
                                currentIndex = utils.prefNetworkInfIndex()
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: !settings.allowNotIsomMp4
                            text: qsTr("Block fragmented MP4 audio streams")
                            description: qsTr("Some UPnP devices don't support audio stream in fragmented MP4 format. " +
                                              "This kind of stream might even hang a device. " +
                                              "To overcome this problem, Jupii tries to re-transcode stream to standard MP4. " +
                                              "When re-transcoding fails and this option is enabled, item will not be played at all.")
                            onClicked: {
                                settings.allowNotIsomMp4 = !settings.allowNotIsomMp4
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.showAllDevices
                            text: qsTr("All devices visible")
                            description: qsTr("All types of UPnP devices are detected " +
                                              "and shown, including unsupported devices like " +
                                              "home routers. For unsupported devices only " +
                                              "basic description information is available. " +
                                              "This option might be useful for auditing UPnP devices " +
                                              "in your local network.")
                            onClicked: {
                                settings.showAllDevices = !settings.showAllDevices
                            }
                        }

                        /*ComboBox {
                            label: qsTr("Video encoder")
                            currentIndex: {
                                switch (settings.casterVideoEncoder) {
                                case Settings.CasterVideoEncoder_Auto: return 0;
                                case Settings.CasterVideoEncoder_X264: return 1;
                                case Settings.CasterVideoEncoder_Nvenc: return 2;
                                case Settings.CasterVideoEncoder_V4l2: return 3;
                                }
                                return 0;
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("Auto") }
                                MenuItem { text: "x264" }
                                MenuItem { text: "nvenc" }
                                MenuItem { text: "V4L2" }
                            }

                            onCurrentIndexChanged: {
                                switch (currentIndex) {
                                case 0: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto; break;
                                case 1: settings.casterVideoEncoder = Settings.CasterVideoEncoder_X264; break;
                                case 2: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Nvenc; break;
                                case 3: settings.casterVideoEncoder = Settings.CasterVideoEncoder_V4l2; break;
                                default: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto;
                                }
                            }
                        }*/

                        TextField {
                            anchors {
                                left: parent.left; right: parent.right
                            }
                            label: qsTr("Frontier Silicon PIN")
                            text: settings.fsapiPin
                            inputMethodHints: Qt.ImhDigitsOnly
                            placeholderText: qsTr("Enter Frontier Silicon PIN")
                            onTextChanged: {
                                settings.fsapiPin = text
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.logToFile
                            text: qsTr("Enable logging")
                            description: qsTr("Needed for troubleshooting purposes. " +
                                              "The log data is stored in %1 file.")
                                                .arg(settings.getCacheDir() + "/jupii.log")
                            onClicked: {
                                settings.logToFile = !settings.logToFile
                            }
                        }

                        Spacer {}

                        Button {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Reset tips and hints")
                            onClicked: {
                                remorse.execute(qsTr("Resetting tips and hints"), function() { settings.resetHints() } )
                            }
                        }

                        Spacer {}

                        Button {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Reset settings")
                            onClicked: {
                                remorse.execute(qsTr("Resetting settings"), function() { settings.reset() } )
                            }
                        }

                        Spacer {}
                        Spacer {}
                    }
                }
            }
        }
    }

    RemorsePopup {
        id: remorse
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
