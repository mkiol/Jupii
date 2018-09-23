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
                color: root.ok ? Theme.primaryColor : "red"

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
                placeholderText: qsTr("Optionally enter Name")
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
                text: qsTr("Only HTTP URLs are supported. If URL points to a playlist file, first playlist item will be used. Only PLS playlists are supported right now. For internet radio URLs, MP3 streams should be preferred because they are the most widely supported by UPnP devices. If Name is not provided, it will be discover automatically from the stream meta data.");
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }
}
