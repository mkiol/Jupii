/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

import harbour.jupii.DeviceInfo 1.0

Kirigami.ScrollablePage {
    id: root

    property alias udn: deviceInfo.udn

    Layout.fillWidth: true

    title: qsTr("Device description")

    actions {
        main: Kirigami.Action {
            iconName: "edit-copy"
            text: qsTr("Copy XML description")
            onTriggered: {
               utils.setClipboard(deviceInfo.getXML())
               showPassiveNotification(qsTr("Description was copied to clipboard."))
            }
        }
    }

    DeviceInfo {
        id: deviceInfo
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Image {
            Layout.alignment: Qt.AlignLeft
            Layout.preferredWidth: Kirigami.Units.iconSizes.enormous
            Layout.preferredHeight: Kirigami.Units.iconSizes.enormous
            fillMode: Image.PreserveAspectCrop
            source: deviceInfo.icon
            visible: status === Image.Ready
        }

        GridLayout {
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            rowSpacing: Kirigami.Units.largeSpacing
            columnSpacing: Kirigami.Units.largeSpacing
            columns: 2

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Name")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: deviceInfo.friendlyName
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Device type")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: deviceInfo.deviceType
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Model name")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: deviceInfo.modelName
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Manufacturer")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: deviceInfo.manufacturer
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("UDN")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: deviceInfo.udn
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("URL")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: deviceInfo.urlBase
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Services")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: deviceInfo.services.join("\n")
            }
        }
    }
}
