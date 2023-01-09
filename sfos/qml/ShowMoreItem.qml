/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root
    signal clicked()

    opacity: enabled ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity { FadeAnimation {} }
    height: Theme.itemSizeMedium
    width: parent.width

    Button {
        anchors.centerIn: parent
        text: qsTr("Show more")
        onClicked: root.clicked()
    }
}
