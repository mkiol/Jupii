/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0

BusyIndicator {
    property alias text: label.text
    anchors.centerIn: parent
    size: BusyIndicatorSize.Large

    onRunningChanged: {
        if (!running) label.text = ""
    }

    Label {
        id: label
        enabled: text.length > 0
        opacity: enabled ? 1.0 : 0.0
        visible: opacity > 0.0
        Behavior on opacity { FadeAnimation {} }
        anchors.centerIn: parent
        font.pixelSize: Theme.fontSizeMedium
        color: parent.color
    }
}
