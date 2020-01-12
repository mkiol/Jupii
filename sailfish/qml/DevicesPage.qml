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

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                     Theme.itemSizeSmall : Theme.itemSizeLarge

    function connectDevice(deviceId) {
        if (deviceId) {
            rc.init(deviceId)
            av.init(deviceId)
        }
        createPlaylistPage()
        pageStack.navigateForward()
    }

    function createPlaylistPage() {
        if (pageStack.currentPage === root && pageStack.depth === 1) {
            pageStack.pushAttached(Qt.resolvedUrl("MediaRendererPage.qml"))
        }
    }

    Component.onCompleted: {
        directory.discover()
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            createPlaylistPage()
        }
    }

    Connections {
        target: directory

        onError: {
            switch(code) {
            case 1:
                notification.show(qsTr("Cannot connect to the local network"))
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

        model: devmodel

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
                text: directory.inited ? qsTr("Find devices") : qsTr("Connect")
                enabled: !directory.busy
                onClicked: {
                    if (directory.inited)
                        directory.discover()
                    else
                        directory.init()
                }
            }

            /*MenuItem {
                visible: !av.busy && !rc.busy
                text: qsTr("Show playlist")
                onClicked: {
                    connectDevice()
                }
            }*/
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

            onMenuOpenChanged: {
                if (menuOpen)
                    directory.xcGetStatus(model.id)
            }

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Connect")
                    enabled: !model.active
                    visible: model.supported
                    onClicked: {
                        connectDevice(model.id)
                    }
                }

                MenuItem {
                    text: qsTr("Show description")
                    onClicked: {
                        pageStack.push(Qt.resolvedUrl("DeviceInfoPage.qml"),
                                       {udn: model.id})
                    }
                }

                MenuItem {
                    text: model.power ? qsTr("Power Off") : qsTr("Power On")
                    visible: model.xc
                    onClicked: {
                        directory.xcTogglePower(model.id)
                    }
                }

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

            onClicked: {
                if (model.supported) {
                    connectDevice(model.id)
                } else {
                    pageStack.push(Qt.resolvedUrl("DeviceInfoPage.qml"),
                                   {udn: model.id})
                }
            }
        }

        ViewPlaceholder {
            enabled: !directory.busy && (listView.count == 0 || !directory.inited)
            text: directory.inited ?
                      qsTr("No devices found. \n" +
                           "Pull down to find more devices in your network.") :
                      qsTr("Pull down to connect to the local network.")
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
