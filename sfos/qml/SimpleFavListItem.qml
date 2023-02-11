/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: root

    property alias title: _title
    property alias icon: _icon
    property alias defaultIcon: _dicon
    property bool active: false
    property bool fav: false
    property bool showFavOption: true

    readonly property bool _iconDisabled: _icon.status !== Image.Ready &&
                                         _dicon.status !== Image.Ready

    signal favClicked

    opacity: enabled && _icon.status !== Image.Loading ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity { FadeAnimation {} }

    contentHeight: Theme.itemSizeMedium

    anchors {
        left: parent.left
        right: parent.right
    }

    Image {
        id: _icon
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        height: Theme.iconSizeMedium
        width: Theme.iconSizeMedium

        sourceSize.height: Theme.iconSizeMedium
        sourceSize.width: Theme.iconSizeMedium

        Image {
            id: _dicon
            anchors.fill: parent
            visible: icon.status !== Image.Ready
        }
    }

    Label {
        id: _title

        truncationMode: TruncationMode.Fade

        anchors {
            left: _iconDisabled ? parent.left : _icon.right
            right: _icon_fav.left
            leftMargin: _iconDisabled ? Theme.horizontalPageMargin : Theme.paddingMedium
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        color: root.highlighted || root.active ? Theme.highlightColor : Theme.primaryColor
    }

    Image {
        id: _icon_fav
        visible: root.showFavOption || root.fav

        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        height: Theme.iconSizeSmallPlus
        width: Theme.iconSizeSmallPlus

        source: (root.fav ? "image://theme/icon-m-favorite-selected?" :
                            "image://theme/icon-m-favorite?") +
                (root.highlighted || root.active ? Theme.highlightColor : Theme.primaryColor)

        MouseArea {
            anchors.fill: parent
            onClicked: favClicked()
        }
    }
}
