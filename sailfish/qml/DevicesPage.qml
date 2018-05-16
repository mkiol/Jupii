/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.DeviceModel 1.0

Page {
    id: root

    property real preferredItemHeight: root && root.isLandscape ? Theme.itemSizeSmall : Theme.itemSizeLarge

    Component.onCompleted: {
        //directory.discoverFavs()
        directory.discover()
    }

    Connections {
        target: directory

        onError: {
            switch(code) {
            case 1:
                notification.show(qsTr("Can't connect to the local network"))
                break
            default:
                notification.show(qsTr("An internal error occurred"))
            }
        }

        onInitedChanged: {
            if (directory.inited) {
                directory.discover()
            } else {
                pageStack.pop(root)
            }
        }
    }

    SilicaListView {
        id: listView
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: root.height
        clip: true

        model: DeviceModel {}

        header: PageHeader {
            visible: directory.inited
            title: qsTr("Devices")
        }

        PullDownMenu {
            id: menu

            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }

            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }

            MenuItem {
                text: qsTr("Add device manually")
                visible: !directory.busy
                enabled: !directory.busy && directory.inited
                onClicked: pageStack.push(Qt.resolvedUrl("AddDevicePage.qml"))
            }

            MenuItem {
                text: directory.inited ? qsTr("Find devices") : qsTr("Connect")
                enabled: !directory.busy
                onClicked: {
                    if (directory.inited)
                        directory.discover()
                    else
                        directory.init()
                }
            }
        }

        delegate: SimpleListItem {
            title.text: model.title
            icon.source: model.icon
            defaultIcon.source: "image://icons/icon-m-device?" + (highlighted ?
                                    Theme.highlightColor : Theme.primaryColor)
            visible: !directory.busy && directory.inited

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Connect")
                    visible: model.supported
                    onClicked: {
                        pageStack.push(Qt.resolvedUrl("MediaRendererPage.qml"),
                                       {deviceId: model.id, deviceName: model.title})
                    }
                }

                MenuItem {
                    text: qsTr("Show description")
                    onClicked: {
                        pageStack.push(Qt.resolvedUrl("DeviceInfoPage.qml"),{udn: model.id})
                    }
                }
            }

            onClicked: {
                if (model.supported)
                    pageStack.push(Qt.resolvedUrl("MediaRendererPage.qml"),
                                   {deviceId: model.id, deviceName: model.title})
                else
                    pageStack.push(Qt.resolvedUrl("DeviceInfoPage.qml"),{udn: model.id})
            }
        }

        ViewPlaceholder {
            enabled: !directory.busy && (listView.count == 0 || !directory.inited)
            text: directory.inited ?
                      qsTr("No devices found. \n" +
                           "Pull down to find more devices in your network.") :
                      qsTr("Not connected. \n" +
                           "Pull down to connect to the local network.")
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: directory.busy
        size: BusyIndicatorSize.Large
    }

    VerticalScrollDecorator {
        flickable: listView
    }
}
