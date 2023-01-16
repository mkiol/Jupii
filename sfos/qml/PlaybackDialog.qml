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
    canAccept: settings.casterPlaybacks.length !== 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    Component.onCompleted: {
        settings.discoverCasterSources()
    }

    onAccepted: {
        playlist.addItemUrl("jupii://playback")
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
                title: qsTr("Add audio capture")
            }

            InlineMessage {
                visible: settings.casterPlaybacks.length === 0
                width: parent.width
                text: qsTr("Could not find any audio source to capture.")
            }

            ComboBox {
                enabled: settings.casterPlaybacks.length !== 0

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
                enabled: settings.casterPlaybacks.length !== 0

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

