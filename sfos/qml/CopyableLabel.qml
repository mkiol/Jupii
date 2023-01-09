/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: root

    property alias text: label.text

    menu: contextMenu

    contentHeight: label.height + 2 * Theme.paddingMedium

    Label {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        x: Theme.horizontalPageMargin
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        width: parent.width - 2 * x
        color: highlighted ? Theme.highlightColor : Theme.primaryColor
        font.pixelSize: Theme.fontSizeSmall
    }

    Component {
        id: contextMenu
        ContextMenu {
            MenuItem {
                text: qsTr("Copy")
                onClicked: {
                    Clipboard.text = root.text
                }
            }
        }
    }
}
