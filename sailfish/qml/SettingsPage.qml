/* Copyright (C) 2017-2019 Michal Kosciesza <michal@mkiol.net>
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

            /*Slider {
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
            }*/

            TextSwitch {
                automaticCheck: false
                checked: settings.useHWVolume
                text: qsTr("Volume control with hardware keys")
                onClicked: {
                    settings.useHWVolume = !settings.useHWVolume
                }
            }

            Slider {
                visible: settings.useHWVolume
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

            Slider {
                width: parent.width
                minimumValue: 1
                maximumValue: 100
                stepSize: 1
                handleVisible: true
                value: Math.round(settings.micVolume)
                valueText: value
                label: qsTr("Microphone sensitivity")

                onValueChanged: {
                    settings.micVolume = value
                }
            }

            /*Slider {
                width: parent.width
                minimumValue: 1
                maximumValue: 10
                stepSize: 1
                handleVisible: true
                value: Math.round(settings.audioBoost)
                valueText: value
                label: qsTr("Audio Capture volume boost")

                onValueChanged: {
                    settings.audioBoost = value
                }
            }*/

            ExpandingSectionGroup {
                ExpandingSection {
                    title: qsTr("Experiments")

                    content.sourceComponent: Column {
                        /*ComboBox {
                            label: qsTr("Screen capture")
                            currentIndex: settings.screenSupported ?
                                              settings.screenAudio ? 2 : 1 : 0

                            menu: ContextMenu {
                                MenuItem { text: qsTr("Disabled") }
                                MenuItem { text: qsTr("Enabled") }
                                MenuItem { text: qsTr("Enabled with audio") }
                            }

                            onCurrentIndexChanged: {
                                if (currentIndex === 2) {
                                    settings.screenSupported = true;
                                    settings.screenAudio = true;
                                } else if (currentIndex === 1) {
                                    settings.screenSupported = true;
                                    settings.screenAudio = false;
                                } else {
                                    settings.screenSupported = false;
                                    settings.screenAudio = false;
                                }
                            }

                            description: qsTr("Enables Screen casting feature. Capturing " +
                                              "video is still in beta stage, so " +
                                              "the quality may be not of the best.");
                        }

                        ComboBox {
                            visible: settings.screenSupported
                            label: qsTr("Force screen 16:9 aspect ratio")
                            currentIndex: settings.screenCropTo169
                            menu: ContextMenu {
                                MenuItem { text: qsTr("Don't force") }
                                MenuItem { text: qsTr("Scale") }
                                MenuItem { text: qsTr("Crop") }
                            }

                            onCurrentIndexChanged: {
                                settings.screenCropTo169 = currentIndex
                            }
                        }

                        Slider {
                            visible: settings.screenSupported
                            width: parent.width
                            minimumValue: 1
                            maximumValue: 5
                            stepSize: 1
                            handleVisible: true
                            value: settings.screenQuality
                            valueText: value
                            label: qsTr("Screen capture quality")

                            onValueChanged: {
                                settings.screenQuality = value
                            }
                        }*/

                        TextSwitch {
                            automaticCheck: false
                            visible:  settings.remoteContentMode == 0
                            checked: settings.rec
                            text: qsTr("Stream recorder")
                            description: qsTr("Enables audio recording from URL items. " +
                                              "If URL item is a Icecast stream, individual tracks from a stream will be recorded. " +
                                              "To enable recording use 'Record' button located on the bottom bar. " +
                                              "When the 'Record' button is activated before " +
                                              "the end of currently played track, the whole track is saved to a file.")
                            onClicked: {
                                settings.rec = !settings.rec
                            }
                        }

                        ListItem {
                            visible: settings.rec && settings.remoteContentMode == 0
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

                        Spacer {}
                        Spacer {}
                    }
                }

                ExpandingSection {
                    title: qsTr("Advanced")
                    content.sourceComponent: Column {
                        ComboBox {
                            visible: settings.isDebug()
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

                        ComboBox {
                            label: qsTr("Stream relaying")
                            description: qsTr("Internet streams are relayed to UPnP device through %1. " +
                                              "Recommended option is 'Always' because it provides best " +
                                              "compatibility. When relaying is disabled ('Never' option), " +
                                              "Icecast titles and Stream recorder " +
                                              "are not available.").arg(APP_NAME)
                            currentIndex: {
                                // 0 - proxy for all
                                // 1 - redirection for all
                                // 2 - none for all
                                // 3 - proxy for shoutcast, redirection for others
                                // 4 - proxy for shoutcast, none for others
                                if (settings.remoteContentMode == 0)
                                    return 0
                                if (settings.remoteContentMode == 1 || settings.remoteContentMode == 2)
                                    return 2
                                if (settings.remoteContentMode == 3 || settings.remoteContentMode == 4)
                                    return 1
                                return 0
                            }


                            menu: ContextMenu {
                                MenuItem { text: qsTr("Always") }
                                MenuItem { text: qsTr("Only Icecast") }
                                MenuItem { text: qsTr("Never") }
                            }
                            onCurrentIndexChanged: {
                                if (currentIndex == 0)
                                    settings.remoteContentMode = 0
                                else if (currentIndex == 1)
                                    settings.remoteContentMode = 4
                                else if (currentIndex == 2)
                                    settings.remoteContentMode = 2
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
                            visible: settings.isDebug() && settings.screenSupported
                            label: qsTr("Screen capture encoder")
                            currentIndex: {
                                var enc = settings.screenEncoder;
                                if (enc === "libx264")
                                    return 1;
                                if (enc === "libx264rgb")
                                    return 2;
                                if (enc === "h264_omx")
                                    return 3;
                                return 0;
                            }
                            menu: ContextMenu {
                                MenuItem { text: qsTr("Auto") }
                                MenuItem { text: "libx264" }
                                MenuItem { text: "libx264rgb" }
                                MenuItem { text: "h264_omx" }
                            }

                            onCurrentIndexChanged: {
                                if (currentIndex === 1)
                                    settings.screenEncoder = "libx264";
                                else if (currentIndex === 2)
                                    settings.screenEncoder = "libx264rgb";
                                else if (currentIndex === 3)
                                    settings.screenEncoder = "h264_omx";
                                else
                                    settings.screenEncoder = "";
                            }
                        }

                        ComboBox {
                            visible: settings.isDebug() && settings.screenSupported
                            label: "Screen capture framerate"
                            currentIndex: {
                                if (settings.screenFramerate < 15) {
                                    return 0;
                                } else if (settings.screenFramerate < 30) {
                                    return 1;
                                } else {
                                    return 2;
                                }
                            }
                            menu: ContextMenu {
                                MenuItem { text: "5 fps (default)" }
                                MenuItem { text: "15 fps" }
                                MenuItem { text: "30 fps" }
                            }

                            onCurrentIndexChanged: {
                                switch (currentIndex) {
                                case 0:
                                    settings.screenFramerate = 5; break;
                                case 1:
                                    settings.screenFramerate = 15; break;
                                case 2:
                                    settings.screenFramerate = 30; break;
                                default:
                                    settings.screenFramerate = 5;
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
                                                .arg(utils.homeDirPath() + "/jupii.log")
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
