/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import org.kde.kirigami 2.14 as Kirigami

Item {
    id: root

    property alias running: busyIndicator.running
    property alias text: label.text

    parent: root.overlay
    anchors.centerIn: parent
    width: busyIndicator.width
    height: busyIndicator.height + label.height

    Controls.BusyIndicator {
        id: busyIndicator
        anchors.horizontalCenter: parent.horizontalCenter
        width: Kirigami.Units.iconSizes.large
        height: Kirigami.Units.iconSizes.large
    }

    Controls.Label {
        id: label
        anchors.top: busyIndicator.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        //color: Kirigami.Theme.disabledTextColor
        font: Kirigami.Theme.defaultFont
        visible: busyIndicator.running
    }
}
