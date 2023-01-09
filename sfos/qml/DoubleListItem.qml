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

    property alias title: _title
    property alias subtitle: _subtitle
    property alias icon: _icon
    property alias defaultIcon: _dicon
    property alias attachedIcon: _aicon
    property alias attachedIcon2: _aicon2
    property alias extra: extraLabel.text
    property alias extra2: extraLabel2.text
    property bool dimmed: false

    readonly property bool _iconDisabled: _icon.status !== Image.Ready &&
                                         _dicon.status !== Image.Ready

    opacity: enabled && _icon.status !== Image.Loading ? 1.0 : dimmed ? 0.7 : 0.0
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

        Column {
            width: Theme.iconSizeSmall
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            AttachedIcon {
                id: _aicon
            }

            AttachedIcon {
                id: _aicon2
            }
        }
    }

    Column {
        width: parent.width

        anchors {
            left: _iconDisabled ? parent.left : _icon.right
            right: parent.right
            rightMargin: Theme.horizontalPageMargin +
                         (extraLabel.visible || extraLabel2.visible ? extraCol.width + Theme.paddingMedium : 0)
            leftMargin: _iconDisabled ? Theme.horizontalPageMargin : Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }

        Label {
            id: _title

            anchors {
                left: parent.left
                right: parent.right
            }

            truncationMode: TruncationMode.Fade
            color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
        }

        Label {
            id: _subtitle
            visible: text.length > 0

            anchors {
                left: parent.left
                right: parent.right
            }

            font.pixelSize: Theme.fontSizeSmall
            truncationMode: TruncationMode.Fade
            color: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }
    }

    Column {
        id: extraCol
        spacing: Theme.paddingSmall
        width: Math.max(extraLabel.width, extraLabel2.width)
        anchors.right: parent.right
        anchors.rightMargin: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter

        ExtraLabel {
            id: extraLabel
            anchors.right: parent.right
            fontPixelSize: Theme.fontSizeSmall
            textColor: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }

        ExtraLabel {
            id: extraLabel2
            anchors.right: parent.right
            fontPixelSize: Theme.fontSizeTiny
            textColor: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
        }
    }
}
