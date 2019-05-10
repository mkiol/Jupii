/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.DeviceInfo 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    property alias udn: deviceInfo.udn

    DeviceInfo {
        id: deviceInfo
    }

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Device description")
            }

            PullDownMenu {
                id: menu
                enabled: settings.showAllDevices
                visible: enabled

                MenuItem {
                    text: qsTr("Copy XML description")
                    enabled: settings.showAllDevices
                    visible: enabled
                    onClicked: {
                        Clipboard.text = deviceInfo.getXML()
                        notification.show(qsTr("Description copied to the clipboard"))
                    }
                }
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.iconSizeExtraLarge
                height: Theme.iconSizeExtraLarge
                source: deviceInfo.icon

                enabled: status === Image.Ready
                opacity: enabled ? 1.0 : 0.0
                visible: opacity > 0.0
                Behavior on opacity { FadeAnimation {} }
            }

            Column {
                width: parent.width
                spacing: Theme.paddingMedium

                DetailItem {
                    label: qsTr("Name")
                    value: deviceInfo.friendlyName
                }

                DetailItem {
                    label: qsTr("Device type")
                    value: deviceInfo.deviceType
                }

                DetailItem {
                    label: qsTr("Model name")
                    value: deviceInfo.modelName
                }

                DetailItem {
                    label: qsTr("Manufacturer")
                    value: deviceInfo.manufacturer
                }

                DetailItem {
                    label: qsTr("UDN")
                    value: deviceInfo.udn
                }

                DetailItem {
                    label: qsTr("URL")
                    value: deviceInfo.urlBase
                }
            }

            SectionHeader {
                text: qsTr("Services")
            }

            Repeater {
                model: deviceInfo.services

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                    }
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeSmall
                    text: modelData
                    color: Theme.highlightColor
                }
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
