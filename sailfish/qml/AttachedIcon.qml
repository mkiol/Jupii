/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Rectangle {
    id: root

    property alias source: _icon.source

    visible: _icon.status === Image.Ready
    width: Theme.iconSizeSmall
    height: Theme.iconSizeSmall
    color: Theme.rgba((Theme.colorScheme === Theme.LightOnDark ?
              Theme.darkPrimaryColor : Theme.lightPrimaryColor), 0.5)
    Image {
        id: _icon
        anchors.fill: parent
    }
}
