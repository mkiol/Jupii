/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0

Rectangle {
    property bool highlighted: false

    color: highlighted ? Theme.highlightColor : Theme.primaryColor
    Behavior on color { FadeAnimation { duration: 200 } }
    enabled: false
    opacity: enabled ? highlighted ? 0.5 : 0.2 : 0.0
    visible: opacity > 0.0
    Behavior on opacity { FadeAnimation { duration: 100 } }
    width: Theme.itemSizeMedium
    height: Theme.paddingMedium
    anchors.horizontalCenter: parent.horizontalCenter
    y: -height/2
    radius: 20
}
