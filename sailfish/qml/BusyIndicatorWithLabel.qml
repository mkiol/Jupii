/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0

Item {
    id: root
    property alias running: busyIndicator.running
    property alias size: busyIndicator.size
    property alias infoText: infoLabel.text
    property alias progressText: progressLabel.text

    anchors.centerIn: parent
    height: column.height
    visible: opacity > 0.0
    opacity: running ? 1.0 : 0.0
    Behavior on opacity { FadeAnimation {} }

    Column {
        id: column
        width: root.width
        spacing: Theme.paddingMedium
        anchors.verticalCenter: parent.verticalCenter

        InfoLabel {
            id: infoLabel
            visible: text.length !== 0
        }

        BusyIndicator {
            id: busyIndicator
            anchors.horizontalCenter: parent.horizontalCenter
            size: BusyIndicatorSize.Large

            onRunningChanged: {
                if (!running) progressLabel.text = ""
            }

            Label {
                id: progressLabel
                enabled: text.length !== 0
                opacity: enabled ? 1.0 : 0.0
                visible: opacity > 0.0
                Behavior on opacity { FadeAnimation {} }
                anchors.centerIn: parent
                font.pixelSize: Theme.fontSizeMedium
                color: parent.color
            }
        }
    }
}
