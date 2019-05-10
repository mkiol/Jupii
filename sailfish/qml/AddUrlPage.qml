/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property alias url: urlField.text
    property alias name: nameField.text
    property bool ok: utils.isUrlOk(urlField.text.trim())

    canAccept: ok

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingLarge

            DialogHeader {
                acceptText: qsTr("Add URL")
            }

            TextField {
                id: urlField
                width: parent.width
                placeholderText: qsTr("Enter URL")
                label: qsTr("URL")
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText

                EnterKey.iconSource: nameField.text.length > 0 && root.ok ?
                                         "image://theme/icon-m-enter-accept" :
                                         "image://theme/icon-m-enter-next"
                EnterKey.onClicked: {
                    if (nameField.text.length > 0 && root.ok)
                        root.accept();
                    else
                        nameField.forceActiveFocus();
                }

            }

            TextField {
                id: nameField
                width: parent.width
                placeholderText: qsTr("Enter Name (optional)")
                label: qsTr("Name")

                EnterKey.iconSource: root.ok ? "image://theme/icon-m-enter-accept" :
                                               "image://theme/icon-m-enter-next"
                EnterKey.onClicked: {
                    if (ok)
                        root.accept();
                    else
                        urlField.forceActiveFocus()
                }
            }

            Tip {
                text: qsTr("Only HTTP URLs are supported. If URL points to a playlist file, first playlist item will be added. If Name is not provided, it will be discovered automatically based on stream meta data.");
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
