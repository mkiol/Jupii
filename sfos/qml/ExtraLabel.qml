/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Rectangle {
    id: root
    property alias text: label.text
    property alias textColor: label.color
    property alias fontPixelSize: label.font.pixelSize

    width: visible ? label.implicitWidth + 2 * Theme.paddingSmall : 0
    height: visible ? label.implicitHeight + Theme.paddingSmall : 0
    visible: label.text.length > 0
    color: "transparent"
    border.color: label.color
    radius: 8
    Label {
        id: label
        font.pixelSize: Theme.fontSizeSmall
        color: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        anchors.centerIn: parent
    }
}
