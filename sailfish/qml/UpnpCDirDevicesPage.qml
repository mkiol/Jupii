/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
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

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property bool _doPop: false

    signal accepted(var items);

    function doPop() {
        if (pageStack.busy)
            _doPop = true
        else
            pageStack.pop(pageStack.previousPage(root))
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop) {
                root._doPop = false
                pageStack.pop()
            }
        }
    }

    Component.onCompleted: {
        devmodel.deviceType = DeviceModel.MediaServerType
        directory.discover()
    }

    Component.onDestruction: {
        devmodel.deviceType = DeviceModel.MediaRendererType
    }

    Connections {
        target: directory

        onError: {
            switch(code) {
            case 1:
                notification.show(qsTr("Cannot connect to a local network"))
                break
            default:
                notification.show(qsTr("An internal error occurred"))
            }
        }

        onInitedChanged: {
            if (directory.inited) {
                directory.discover()
            }
        }
    }

    SilicaListView {
        id: listView
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: root.height
        clip: true

        model: devmodel

        header: PageHeader {
            title: qsTr("Media Servers")
        }

        PullDownMenu {
            id: menu
            busy: directory.busy

            MenuItem {
                text: qsTr("Find Media Servers")
                enabled: directory.inited && !directory.busy
                onClicked: {
                    directory.discover()
                }
            }
        }

        delegate: SimpleFavListItem {
            title.text: model.title
            icon.source: model.icon
            active: model.active
            fav: model.fav
            defaultIcon.source: "image://icons/icon-m-device?" +
                                (highlighted || model.active ?
                                Theme.highlightColor : Theme.primaryColor)
            visible: directory.inited

            onFavClicked: {
                if (model.fav)
                    settings.removeFavDevice(model.id)
                else
                    settings.addFavDevice(model.id)
            }

            onClicked: {
                cdir.init(model.id)
                var dialog = pageStack.push(Qt.resolvedUrl("UpnpCDirPage.qml"))
                dialog.accepted.connect(function() {
                    root.accepted(dialog.selectedItems)
                    root.doPop()
                })
            }

            menu: ContextMenu {
                MenuItem {
                    text: model.fav ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                    onClicked: {
                        if (model.fav)
                            settings.removeFavDevice(model.id)
                        else
                            settings.addFavDevice(model.id)
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: !directory.busy && (listView.count == 0 || !directory.inited)
            text: directory.inited ?
                      qsTr("No Media Servers found") : qsTr("Disconnected")
            hintText: directory.inited ?
                          qsTr("Pull down to find Media Servers in your network") :
                          qsTr("Connect WLAN to find Media Servers in your network")
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

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
