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

    property var playlist

    canAccept: textField.text.trim().length > 0

    onAccepted: {
        if (playlist.saveToFile(textField.text.trim()))
            notification.show(qsTr("Playlist was saved"))
    }

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingLarge

            DialogHeader {
                acceptText: qsTr("Save")
            }

            TextField {
                id: textField
                //x: Theme.horizontalPageMargin
                width: parent.width
                placeholderText: qsTr("Name")
                label: qsTr("Name")
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }
}
