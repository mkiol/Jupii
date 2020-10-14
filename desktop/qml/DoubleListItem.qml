/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.11 as Kirigami

Kirigami.SwipeListItem {
    id: root

    property alias label: labelItem.text
    property alias subtitle: subtitleItem.text
    property string iconSource
    property alias defaultIconSource: iconItem.fallback
    property alias attachedIconName: attachedIcon.source
    property alias iconSize: iconItem.size
    property bool active: false
    property alias next: nextIcon.visible
    property alias busy: busyIndicator.running

    activeTextColor: Kirigami.Theme.activeTextColor

    width: parent ? parent.width : implicitWidth
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    contentItem: RowLayout {
        id: layout
        Controls.BusyIndicator {
            id: busyIndicator
            running: false
            visible: running
            Layout.minimumHeight: root.iconSize
            Layout.maximumHeight: root.iconSize
            Layout.minimumWidth: root.iconSize
            Layout.maximumWidth: root.iconSize
        }
        Kirigami.Icon {
            id: iconItem
            //Layout.alignment: Qt.AlignVCenter
            property int size: Kirigami.Units.iconSizes.smallMedium
            source: iconSource.length == 0 ? defaultIconSource : iconSource
            Layout.minimumHeight: size
            Layout.maximumHeight: size
            Layout.minimumWidth: size
            Layout.maximumWidth: size
            visible: !root.busy && source != undefined
            //placeholder: defaultIconSource

            Item {
                visible: attachedIconName != undefined && attachedIconName.length > 0
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: Kirigami.Units.iconSizes.small
                height: Kirigami.Units.iconSizes.small
                Rectangle {
                    anchors.fill: parent
                    color: Kirigami.Theme.backgroundColor
                    opacity: 0.7
                }
                Kirigami.Icon {
                    id: attachedIcon
                    anchors.fill: parent
                }
            }
        }
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Controls.Label {
                id: labelItem
                text: root.text
                Layout.fillWidth: true
                color: iconItem.selected || root.active ? root.activeTextColor : root.textColor
                elide: Text.ElideRight
            }
            Controls.Label {
                id: subtitleItem
                Layout.fillWidth: true
                color: iconItem.selected || root.active ? root.activeTextColor : root.textColor
                elide: Text.ElideRight
                font: Kirigami.Theme.smallFont
                opacity: 0.7
                visible: text.length > 0
            }
        }
        Kirigami.Icon {
            id: nextIcon
            visible: false
            Layout.alignment: Qt.AlignVCenter
            source: "go-next"
            Layout.minimumHeight: root.iconSize
            Layout.maximumHeight: root.iconSize
            Layout.minimumWidth: root.iconSize
        }
    }
}
