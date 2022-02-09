/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0

Rectangle {
    color: opacity < 0.3 ? Theme.highlightColor : Theme.primaryColor
    enabled: false
    opacity: enabled ? 0.3 : 0.0
    visible: opacity > 0.0
    Behavior on opacity { FadeAnimation {} }
    width: Theme.itemSizeMedium
    height: Theme.paddingMedium
    anchors.horizontalCenter: parent.horizontalCenter
    y: -height/2
    radius: 20

    Timer {
        id: timer
        interval: 500
        onTriggered: parent.enabled = false
    }

    function trigger() {
        enabled = true;
        timer.start()
    }
}
