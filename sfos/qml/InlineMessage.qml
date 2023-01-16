/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.Settings 1.0

Rectangle {
    property alias text: label.text

    height: label.height + 2 * Theme.paddingSmall
    color: Theme.rgba(Theme.highlightBackgroundColor, Theme.highlightBackgroundOpacity)

    Label {
        id: label
        anchors.centerIn: parent
        color: Theme.highlightColor
        width: parent.width - 2 * Theme.horizontalPageMargin
        wrapMode: Text.WordWrap
    }
}
