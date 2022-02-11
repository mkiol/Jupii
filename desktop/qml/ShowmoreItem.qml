/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2
import org.kde.kirigami 2.14 as Kirigami

Item {
    id: root

    property string targetPage
    property url artistUrl

    width: parent.width
    height: button.height + 2 * Kirigami.Units.largeSpacing

    Button {
        id: button
        anchors.centerIn: parent
        text: qsTr("Show more")
        onClicked: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl(root.targetPage), {artistPage: root.artistUrl})
        }
    }
}
