/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami
import QtQuick.Dialogs 1.1

import harbour.jupii.Settings 1.0

Controls.Dialog {
    id: root

    modal: true
    focus: true
    standardButtons: settings.casterPlaybacks.length === 0 ? Controls.Dialog.Cancel :
                        Controls.Dialog.Ok | Controls.Dialog.Cancel
    title: qsTr("Add audio capture")

    onOpenedChanged: {
        if (open) {
            settings.discoverCasterSources()
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.FormLayout {
            Layout.fillWidth: true

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Information
                visible: settings.casterPlaybacks.length === 0
                text: qsTr("Could not find any audio source to capture.")
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                enabled: settings.casterPlaybacks.length !== 0

                Kirigami.FormData.label: qsTr("Audio source")
                currentIndex: settings.casterPlaybackIdx

                model: settings.casterPlaybacks

                onCurrentIndexChanged: {
                    settings.casterPlaybackIdx = currentIndex
                }
            }

            CasterSourceVolume {
                enabled: settings.casterPlaybacks.length !== 0

                Kirigami.FormData.label: qsTr("Volume boost")
                volume: settings.casterPlaybackVolume
                onVolumeChanged: settings.casterPlaybackVolume = volume
            }
        }
    }
}
