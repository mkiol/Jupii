/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.DeviceModel 1.0
import harbour.jupii.Settings 1.0

Page {
    id: root

    objectName: "devices"

    allowedOrientations: Orientation.All

    readonly property real preferredItemHeight: root && root.isLandscape ?
                                     Theme.itemSizeSmall : Theme.itemSizeLarge
    readonly property bool devless: av.deviceId.length === 0 && rc.deviceId.length === 0

    function connectDevice(deviceId) {
        if (deviceId) {
            rc.init(deviceId)
            av.init(deviceId)
        }
        createPlaylistPage()
        pageStack.navigateForward()
    }

    function disconnectDevice() {
        rc.deInit()
        av.deInit()
    }

    function createPlaylistPage() {
        if (directory.inited && pageStack.currentPage === root && !forwardNavigation) {
            pageStack.pushAttached(Qt.resolvedUrl("PlayQueuePage.qml"))
        }
    }

    Component.onCompleted: {
        devmodel.deviceType = DeviceModel.MediaRendererType
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            devmodel.deviceType = DeviceModel.MediaRendererType
            createPlaylistPage()
        }
    }

    Connections {
        target: directory
        onInitedChanged: createPlaylistPage()
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: devmodel

        header: PageHeader {
            visible: directory.inited
            title: qsTr("Devices")
        }

        PullDownMenu {
            id: menu
            busy: directory.busy

            MenuItem {
                text: qsTr("About %1").arg(APP_NAME)
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }

            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }

            MenuItem {
                text: qsTr("Find devices")
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

            onMenuOpenChanged: {
                if (menuOpen)
                    directory.xcGetStatus(model.id)
            }

            menu: ContextMenu {
                MenuItem {
                    text: model.active ? qsTr("Disconnect") : qsTr("Connect")
                    visible: model.supported
                    onClicked: {
                        if (model.active)
                            disconnectDevice()
                        else
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
                      qsTr("No devices") : qsTr("Disconnected")
            hintText: directory.inited ?
                          qsTr("Pull down to find more devices in your network") :
                          qsTr("Connect WLAN to find devices in your network")
        }
    }

    InteractionHintLabel_ {
        anchors.bottom: parent.bottom
        opacity: enabled ? 1.0 : 0.0
        Behavior on opacity { FadeAnimation {} }
        text: qsTr("Connect to device or flick left to access play queue")
        enabled: settings.hintEnabled(Settings.Hint_DeviceSwipeLeft) && pageStack.currentPage === root && forwardNavigation && !menu.active
        onEnabledChanged: enabled ? hint.start() : hint.stop()
    }
    TouchInteractionHint {
        id: hint
        interactionMode: TouchInteraction.Swipe
        direction: TouchInteraction.Left
        loops: Animation.Infinite
        anchors.centerIn: parent
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
