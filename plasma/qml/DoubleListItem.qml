/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
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
    property alias attachedIcon2Name: attachedIcon2.source
    property alias iconSize: iconItem.size
    property bool active: false
    property alias next: nextIcon.visible
    property alias busy: busyIndicator.running
    property alias extra: extraLabel.text
    property alias extra2: extra2Label.text
    property real leadingPadding: Kirigami.Units.largeSpacing
    property real trailingPadding: Kirigami.Units.largeSpacing

    activeTextColor: Kirigami.Theme.activeTextColor

    //width: parent ? parent.width : implicitWidth
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    contentItem: Item {
        id: contItem
        implicitWidth: layout.implicitWidth
        implicitHeight: layout.implicitHeight

        RowLayout {
            id: layout
            anchors.left: contItem.left
            anchors.leftMargin: root.leadingPadding
            anchors.right: contItem.right
            anchors.rightMargin: root.trailingPadding

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
                visible: !root.busy && typeof source !== "undefined"

                Column {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: Kirigami.Units.iconSizes.small

                    AttachedIcon {
                        id: attachedIcon
                    }

                    AttachedIcon {
                        id: attachedIcon2
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
            Rectangle {
                Layout.minimumWidth: extra2Label.implicitWidth + 2 * Kirigami.Units.smallSpacing
                Layout.minimumHeight: extra2Label.implicitHeight + Kirigami.Units.smallSpacing
                Layout.alignment: Qt.AlignVCenter
                visible: extra2Label.text.length > 0
                color: "transparent"
                border.color: Kirigami.Theme.disabledTextColor
                radius: 3
                Controls.Label {
                    id: extra2Label
                    color: Kirigami.Theme.disabledTextColor
                    anchors.centerIn: parent
                }
            }
            Rectangle {
                Layout.minimumWidth: extraLabel.implicitWidth + 2 * Kirigami.Units.smallSpacing
                Layout.minimumHeight: extraLabel.implicitHeight + Kirigami.Units.smallSpacing
                Layout.alignment: Qt.AlignVCenter
                visible: extraLabel.text.length > 0
                color: "transparent"
                border.color: Kirigami.Theme.disabledTextColor
                radius: 3
                Controls.Label {
                    id: extraLabel
                    color: Kirigami.Theme.disabledTextColor
                    anchors.centerIn: parent
                }
            }
            Kirigami.Icon {
                id: nextIcon
                visible: false
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                opacity: 0.7
                source: (LayoutMirroring.enabled ? "go-next-symbolic-rtl" : "go-next-symbolic")
                Layout.minimumHeight: root.iconSize
                Layout.maximumHeight: root.iconSize
                Layout.minimumWidth: root.iconSize
                Layout.preferredWidth: root.iconSize
            }
        }
    }
}
