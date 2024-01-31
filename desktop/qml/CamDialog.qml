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

import org.mkiol.jupii.Settings 1.0

Controls.Dialog {
    id: root

    modal: true
    focus: true
    standardButtons: settings.casterCams.length === 0 ? Controls.Dialog.Cancel :
                        Controls.Dialog.Ok | Controls.Dialog.Cancel
    title: qsTr("Add camera")

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
                visible: settings.casterCams.length === 0
                text: qsTr("Could not find any camera connected.")
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Information
                visible: settings.casterCamAudio && settings.casterMics.length === 0
                text: qsTr("Could not find any microphone connected.")
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                enabled: settings.casterCams.length !== 0

                Kirigami.FormData.label: qsTr("Video source")
                currentIndex: settings.casterCamIdx

                model: settings.casterCams

                onCurrentIndexChanged: {
                    settings.casterCamIdx = currentIndex
                }
            }

            Controls.ComboBox {
                enabled: settings.casterCams.length !== 0

                Kirigami.FormData.label: qsTr("Video orientation")
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

                model: [
                    qsTr("Auto"),
                    qsTr("Portrait"),
                    qsTr("Inverted portrait"),
                    qsTr("Landscape"),
                    qsTr("Inverted landscape")
                ]

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

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.Switch {
                enabled: settings.casterCams.length !== 0

                checked: settings.casterCamAudio
                text: qsTr("Capture with audio")
                onToggled: {
                    settings.casterCamAudio = !settings.casterCamAudio
                }
            }

            Controls.ComboBox {
                enabled: settings.casterCamAudio &&
                         settings.casterCams.length !== 0 &&
                         settings.casterMics.length !== 0

                Kirigami.FormData.label: qsTr("Audio source")
                currentIndex: settings.casterMicIdx

                model: settings.casterMics

                onCurrentIndexChanged: {
                    settings.casterMicIdx = currentIndex
                }
            }

            CasterSourceVolume {
                enabled: settings.casterCamAudio &&
                         settings.casterCams.length !== 0 &&
                         settings.casterMics.length !== 0

                Kirigami.FormData.label: qsTr("Volume boost")
                volume: settings.casterMicVolume
                onVolumeChanged: settings.casterMicVolume = volume
            }
        }
    }
}
