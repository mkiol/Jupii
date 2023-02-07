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
    canAccept: settings.casterMics.length !== 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    Component.onCompleted: {
        settings.discoverCasterSources()
    }

    onAccepted: {
        playlist.addItemUrl("jupii://mic")
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
                title: qsTr("Add microphone")
            }

            InlineMessage {
                visible: settings.casterMics.length === 0
                width: parent.width
                text: qsTr("Could not find any microphone connected.")
            }

            ComboBox {
                enabled: settings.casterMics.length !== 0

                label: qsTr("Audio source")

                currentIndex: settings.casterMicIdx

                menu: ContextMenu {
                    Repeater {
                        model: settings.casterMics
                        MenuItem { text: modelData }
                    }
                }

                onCurrentIndexChanged: {
                    settings.casterMicIdx = currentIndex
                }
            }

            CasterSourceVolume {
                enabled: settings.casterMics.length !== 0

                label: qsTr("Volume boost")
                volume: settings.casterMicVolume
                onVolumeChanged: settings.casterMicVolume = volume
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

