/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
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

Controls.Dialog {
    id: root

    property bool ok: utils.isUrlOk(urlField.text.trim())
    property alias url: urlField.text
    property alias name: urlNameField.text
    property alias asAudio: audioSwitch.checked

    modal: true
    focus: true
    standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
    title: qsTr("Add URL")

    onOpenedChanged: {
        if (opened) {
            url = ""
            name = ""
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            Layout.bottomMargin: Kirigami.Units.largeSpacing
            visible: true
            text: qsTr("Only HTTP/HTTPS URLs are supported. If URL points to a playlist file, " +
                       "first playlist item will be added. If URL doesn't point to any media content, " +
                       "youtube-dl will be used to find a direct media URL.")
        }

        Controls.TextField {
            id: urlField
            Layout.fillWidth: true
            Kirigami.FormData.label: qsTr("URL")
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
            placeholderText: qsTr("Enter URL")
            Keys.onReturnPressed: {
                if (urlNameField.text.length > 0 && root.ok)
                    root.accept();
                else
                    urlNameField.forceActiveFocus();
            }
        }

        Controls.TextField {
            id: urlNameField
            Layout.fillWidth: true
            Kirigami.FormData.label: qsTr("Name")
            placeholderText: qsTr("Enter Name (optional)")
            Keys.onReturnPressed: {
                if (root.ok)
                    root.accept();
                else
                    urlField.forceActiveFocus()
            }
        }

        Controls.Switch {
            id: audioSwitch
            Layout.fillWidth: true
            text: qsTr("Add only audio stream")
        }
    }
}
