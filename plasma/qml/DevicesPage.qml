/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

import harbour.jupii.DeviceModel 1.0

Kirigami.ScrollablePage {
    id: root

    Kirigami.Theme.colorSet: Kirigami.Theme.Window

    title: qsTr("Devices")

    onIsCurrentPageChanged: {
        if (isCurrentPage) {
            devmodel.deviceType = DeviceModel.MediaRendererType
        }
    }

    supportsRefreshing: false

    actions {
        main: Kirigami.Action {
            id: refreshAction
            text: qsTr("Find devices")
            iconName: "view-refresh"
            enabled: directory.inited && !directory.busy
            onTriggered: {
                devmodel.deviceType = DeviceModel.MediaRendererType
                directory.discover()
            }
        }
    }

    function connectDevice(deviceId) {
        if (deviceId) {
            rc.init(deviceId)
            av.init(deviceId)
            removePagesAfterPlayQueue();
        }
    }

    function disconnectDevice() {
        rc.deInit()
        av.deInit()
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem

            label: model.title
            defaultIconSource: "computer"
            iconSource: model.icon
            iconSize: Kirigami.Units.iconSizes.medium
            active: model.active
            onClicked: {
                if (model.supported) {
                    connectDevice(model.id)
                } else {
                    descriptionAction.trigger()
                }
            }

            onHoveredChanged: {
                if (hovered && model.xc)
                    directory.xcGetStatus(model.id)
            }

            actions: [
                Kirigami.Action {
                    visible: model.supported
                    iconName: model.active ? "network-disconnect" : "network-connect"
                    text: model.active ? qsTr("Disconnect") : qsTr("Connect")
                    onTriggered: {
                        if (model.active)
                            disconnectDevice()
                        else
                            connectDevice(model.id)
                    }
                },
                Kirigami.Action {
                    iconName: "system-shutdown"
                    text: model.power ? qsTr("Power Off") : qsTr("Power On")
                    visible: model.xc
                    onTriggered: {
                        directory.xcTogglePower(model.id)
                    }
                },
                Kirigami.Action {
                    id: descriptionAction
                    iconName: "description"
                    text: qsTr("Show description")
                    onTriggered: {
                        pageStack.push(Qt.resolvedUrl("DeviceInfoPage.qml"), {udn: model.id})
                    }
                },
                Kirigami.Action {
                    visible: model.supported
                    iconName: model.fav ? "favorite" : "view-media-favorite"
                    text: model.fav ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                    onTriggered: {
                        if (model.fav)
                            settings.removeFavDevice(model.id)
                        else
                            settings.addFavDevice(model.id)
                    }
                }
            ]
        }
    }

    ListView {
        id: itemList
        model: devmodel
        delegate: Kirigami.DelegateRecycler {
            width: parent ? parent.width : implicitWidth
            sourceComponent: listItemComponent
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: !directory.busy && itemList.count === 0
            text: directory.inited ?
                      qsTr("No devices") : qsTr("No network connection")
            helpfulAction: Kirigami.Action {
                enabled: directory.inited
                iconName: "view-refresh"
                text: qsTr("Find devices")
                onTriggered: refreshAction.trigger()
            }
        }
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        parent: root.overlay
        running: directory.busy
    }
}
