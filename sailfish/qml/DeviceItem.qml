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

    property variant devModel

    property string _type: utils.friendlyDeviceType(devModel.type)
    property color _primaryColor: down ? Theme.highlightColor : Theme.primaryColor
    property color _secondaryColor: down ? Theme.secondaryHighlightColor : Theme.secondaryColor
    property bool _fav: devModel.fav

    signal favClicked()

    contentHeight: Theme.itemSizeMedium

    Image {
        id: icon
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        height: Theme.iconSizeMedium
        width: Theme.iconSizeMedium
        source: devModel.icon

        Image {
            anchors.fill: parent
            source: "image://icons/icon-m-device?" + root._primaryColor
            visible: icon.status !== Image.Ready
        }
    }

    /*IconButton {
        id: fav
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        icon.source: root._fav ?
                         "image://theme/icon-m-favorite-selected?" + root._primaryColor :
                         "image://theme/icon-m-favorite?" + root._primaryColor
        onClicked: {
            root.favClicked()
        }
    }*/

    Column {
        anchors {
            left: icon.right
            //right: fav.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            leftMargin: Theme.paddingLarge
            rightMargin: Theme.paddingLarge
        }
        spacing: Theme.paddingSmall

        Label {
            anchors {
                left: parent.left
                right: parent.right
            }
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeMedium
            color: devModel.supported ? root._primaryColor : root._secondaryColor
            text: devModel.title
        }

        /*Label {
            anchors {
                left: parent.left
                right: parent.right
            }
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: root._secondaryColor
            text: root._type
        }*/
    }
}
