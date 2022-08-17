/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import org.kde.kirigami 2.11 as Kirigami

Item {
    id: root

    property alias source: _icon.source

    visible: typeof _icon.source !== "undefined" && _icon.source.length > 0
    width: visible ? Kirigami.Units.iconSizes.small : 0
    height: visible ? Kirigami.Units.iconSizes.small : 0
    Rectangle {
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
        opacity: 0.7
    }
    Kirigami.Icon {
        id: _icon
        anchors.fill: parent
    }
}
