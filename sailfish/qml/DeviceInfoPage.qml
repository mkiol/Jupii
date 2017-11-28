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

    property alias udn: deviceInfo.udn

    DeviceInfo {
        id: deviceInfo
    }

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            anchors {
                left: parent.left
                right: parent.right
            }
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Device description")
            }

            PullDownMenu {
                id: menu

                MenuItem {
                    text: qsTr("Copy XML description")
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
                anchors {
                    left: parent.left
                    right: parent.right
                }
                spacing: Theme.paddingMedium

                AttValue {
                    att: qsTr("Name")
                    value: deviceInfo.friendlyName
                }

                AttValue {
                    att: qsTr("Device type")
                    value: deviceInfo.deviceType
                }

                AttValue {
                    att: qsTr("Model name")
                    value: deviceInfo.modelName
                }

                AttValue {
                    att: qsTr("Manufacturer")
                    value: deviceInfo.manufacturer
                }

                AttValue {
                    att: qsTr("UDN")
                    value: deviceInfo.udn
                }

                AttValue {
                    att: qsTr("URL")
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
}
