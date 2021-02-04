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

import harbour.jupii.DeviceModel 1.0

Kirigami.ScrollablePage {
    id: root
    objectName: "cdir"

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: qsTr("Media Servers")

    //refreshing: directory.busy
    Component.onCompleted: {
        refreshing = Qt.binding(function() { return directory.busy })
        devmodel.deviceType = DeviceModel.MediaServerType
    }
    Component.onDestruction: {
        devmodel.deviceType = DeviceModel.MediaRendererType
    }

    Connections {
        target: directory

        onInitedChanged: {
            devmodel.deviceType = DeviceModel.MediaServerType
            directory.discover()
        }
    }

    actions {
        main: Kirigami.Action {
            id: refreshAction
            text: qsTr("Find Media Servers")
            iconName: "view-refresh"
            enabled: directory.inited && !directory.busy
            onTriggered: {
                devmodel.deviceType = DeviceModel.MediaServerType
                directory.discover()
            }
        }
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem

            label: model.title
            defaultIconSource: "network-server"
            iconSource: model.icon
            iconSize: Kirigami.Units.iconSizes.medium
            next: true
            onClicked: {
                cdir.init(model.id)
                pageStack.push(Qt.resolvedUrl("UpnpCDirPage.qml"))
                highlighted = cdir.deviceId === model.id
            }

            /*actions: [
                Kirigami.Action {
                    iconName: qsTr("folder-open")
                    text: qsTr("Browse")
                    onTriggered: {
                        cdir.init(model.id)
                        pageStack.push(Qt.resolvedUrl("UpnpCDirPage.qml"))
                    }
                },
                Kirigami.Action {
                    iconName: model.fav ? "favorite" : "view-media-favorite"
                    text: model.fav ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                    onTriggered: {
                        if (model.fav)
                            settings.removeFavDevice(model.id)
                        else
                            settings.addFavDevice(model.id)
                    }
                }
            ]*/
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
            text: directory.inited ?
                      qsTr("No Media Servers") : qsTr("No network connection")
            visible: !directory.busy && itemList.count === 0
            helpfulAction: Kirigami.Action {
                enabled: directory.inited
                iconName: "view-refresh"
                text: qsTr("Find devices")
                onTriggered: refreshAction.trigger()
            }
        }
    }
}
