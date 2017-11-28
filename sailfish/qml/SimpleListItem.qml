/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: root

    property string name

    contentHeight: Theme.itemSizeMedium
    anchors {
        left: parent.left
        right: parent.right
    }

    Label {
         text: root.name

         anchors {
             left: parent.left
             right: parent.right
             leftMargin: Theme.horizontalPageMargin;
             rightMargin: Theme.horizontalPageMargin;
             verticalCenter: parent.verticalCenter
         }

         color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
    }
}
