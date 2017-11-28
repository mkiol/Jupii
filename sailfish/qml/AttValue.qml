/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root

    property alias att: _att.text
    property string value: ""
    property alias attColor: _att.color
    property alias valueColor: _value.color
    visible: value.length !== ""

    anchors {
        left: parent.left
        right: parent.right
        leftMargin: Theme.horizontalPageMargin
        rightMargin: Theme.horizontalPageMargin
    }

    height: Math.max(_att.height, _value.height)

    Label {
        id: _att
        x: (parent.width/3) - width
        horizontalAlignment: Text.AlignRight
        wrapMode: Text.WordWrap
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.secondaryHighlightColor
    }

    Label {
        id: _value
        anchors {
            left: _att.right
            right: parent.right
            leftMargin: Theme.paddingMedium
        }
        horizontalAlignment: Text.AlignLeft
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        truncationMode: TruncationMode.Fade
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.highlightColor
        text: root.value === "" ? "-" : root.value
        font.italic: root.value === ""
    }
}
