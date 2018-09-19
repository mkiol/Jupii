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

    property alias url: textField.text

    canAccept: textField.text.trim().length > 0

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
                id: textField
                width: parent.width
                placeholderText: qsTr("Enter URL")
                label: qsTr("URL")
                inputMethodHints: Qt.ImhUrlCharactersOnly
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }
}
