/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.Settings 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All
    canAccept: settings.casterScreens.length !== 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    Component.onCompleted: {
        settings.discoverCasterSources()
    }

    onAccepted: {
        playlist.addItemUrl("jupii://screen")
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Add screen capture")
            }

            InlineMessage {
                visible: settings.casterScreens.length === 0
                width: parent.width
                text: qsTr("Could not find any screen to capture.")
            }

            InlineMessage {
                visible: settings.casterScreenAudio && settings.casterPlaybacks.length === 0
                width: parent.width
                text: qsTr("Could not find any audio source to capture.")
            }

            ComboBox {
                enabled: settings.casterScreens.length !== 0

                label: qsTr("Video source")

                currentIndex: settings.casterScreenIdx

                menu: ContextMenu {
                    Repeater {
                        model: settings.casterScreens
                        MenuItem { text: modelData }
                    }
                }

                onCurrentIndexChanged: {
                    settings.casterScreenIdx = currentIndex
                }
            }

            ComboBox {
                enabled: settings.casterScreens.length !== 0

                label: qsTr("Video orientation")

                currentIndex: {
                    switch (settings.casterVideoOrientation) {
                    case Settings.CasterVideoOrientation_Auto: return 0;
                    case Settings.CasterVideoOrientation_Portrait: return 1;
                    case Settings.CasterVideoOrientation_InvertedPortrait: return 2;
                    case Settings.CasterVideoOrientation_Landscape: return 3;
                    case Settings.CasterVideoOrientation_InvertedLandscape: return 4;
                    }
                    return 0;
                }

                menu: ContextMenu {
                    MenuItem { text: settings.videoOrientationStr(Settings.CasterVideoOrientation_Auto) }
                    MenuItem { text: settings.videoOrientationStr(Settings.CasterVideoOrientation_Portrait) }
                    MenuItem { text: settings.videoOrientationStr(Settings.CasterVideoOrientation_InvertedPortrait) }
                    MenuItem { text: settings.videoOrientationStr(Settings.CasterVideoOrientation_Landscape) }
                    MenuItem { text: settings.videoOrientationStr(Settings.CasterVideoOrientation_InvertedLandscape) }
                }

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0: settings.casterVideoOrientation = Settings.CasterVideoOrientation_Auto; break;
                    case 1: settings.casterVideoOrientation = Settings.CasterVideoOrientation_Portrait; break;
                    case 2: settings.casterVideoOrientation = Settings.CasterVideoOrientation_InvertedPortrait; break;
                    case 3: settings.casterVideoOrientation = Settings.CasterVideoOrientation_Landscape; break;
                    case 4: settings.casterVideoOrientation = Settings.CasterVideoOrientation_InvertedLandscape; break;
                    default: settings.casterVideoOrientation = Settings.CasterVideoOrientation_Auto;
                    }
                }
            }

            TextSwitch {
                enabled: settings.casterScreens.length !== 0

                automaticCheck: false
                checked: settings.casterScreenAudio
                text: qsTr("Capture with audio")

                onClicked: {
                    settings.casterScreenAudio = !settings.casterScreenAudio
                }
            }

            ComboBox {
                enabled: settings.casterScreenAudio && settings.casterPlaybacks.length !== 0

                label: qsTr("Audio source")

                currentIndex: settings.casterPlaybackIdx

                menu: ContextMenu {
                    Repeater {
                        model: settings.casterPlaybacks
                        MenuItem { text: modelData }
                    }
                }

                onCurrentIndexChanged: {
                    settings.casterPlaybackIdx = currentIndex
                }
            }

            CasterSourceVolume {
                enabled: settings.casterScreenAudio && settings.casterPlaybacks.length !== 0

                label: qsTr("Volume")
                volume: settings.casterPlaybackVolume
                onVolumeChanged: settings.casterPlaybackVolume = volume
            }
        }
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
