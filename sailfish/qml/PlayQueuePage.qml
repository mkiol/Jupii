/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0
import harbour.jupii.PlayListModel 1.0
import harbour.jupii.ContentServer 1.0
import harbour.jupii.Settings 1.0

Page {
    id: root

    objectName: "queue"

    allowedOrientations: Orientation.All

    readonly property bool devless: av.deviceId.length === 0 && rc.deviceId.length === 0
    readonly property int itemType: utils.itemTypeFromUrl(av.currentId)
    readonly property bool inited: av.inited && rc.inited
    property bool _doPop: false

    onStatusChanged: {
        if (status === PageStatus.Active) {
            if (devless) {
                settings.disableHint(Settings.Hint_DeviceSwipeLeft)
            }
            updateMediaInfoPage()
            ppanel.update()
        }
    }

    function doPop() {
        if (pageStack.busy)
            _doPop = true;
        else
            pageStack.pop(pageStack.previousPage(root))
    }

    function showActiveItem() {
        if (playlist.activeItemIndex >= 0)
            listView.positionViewAtIndex(playlist.activeItemIndex, ListView.Contain)
        else
            listView.positionViewAtBeginning()
    }

    function showLastItem() {
        listView.positionViewAtEnd();
    }

    function updateMediaInfoPage() {
        if (pageStack.currentPage === root) {
            if (av.controlable) {
                pageStack.pushAttached(Qt.resolvedUrl("MediaInfoPage.qml"))
            } else if (pageStack.depth === 3) {
                pageStack.popAttached(root, PageStackAction.Immediate)
            }
        }
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop) {
                root._doPop = false
                pageStack.pop(pageStack.pop())
            }
        }
    }

    Connections {
        target: av
        onControlableChanged: updateMediaInfoPage()
    }

    Connections {
        target: playlist

        onItemsAdded: root.showLastItem()
        onItemsLoaded: root.showActiveItem()

        onError: {
            if (code === PlayListModel.E_FileExists)
                notifications.show(qsTr("Item is already in play queue"))
            else if (code === PlayListModel.E_ItemNotAdded)
                notifications.show(qsTr("Item cannot be added"))
            else if (code === PlayListModel.E_SomeItemsNotAdded)
                notifications.show(qsTr("Some items cannot be added"))
            else if (code === PlayListModel.E_AllItemsNotAdded)
                notifications.show(qsTr("Items cannot be added"))
            else
                notifications.show(qsTr("Unknown error"))
        }

        onActiveItemChanged: {
            root.showActiveItem()
        }
    }

    SilicaListView {
        id: listView

        width: parent.width
        height: ppanel.open ? ppanel.y : parent.height

        clip: true

        model: playlist

        header: PageHeader {
            title: av.deviceFriendlyName.length > 0 ? av.deviceFriendlyName : qsTr("Play queue")
        }

        VerticalScrollDecorator {}

        PullDownMenu {
            id: menu
            busy: playlist.busy || av.busy || rc.busy || directory.busy || playlist.refreshing

            MenuItem {
                text: qsTr("Save queue")
                visible: !playlist.refreshing && !playlist.busy && listView.count > 0
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("SavePlaylistPage.qml"), {
                                       playlist: playlist
                                   })
                }
            }

            MenuItem {
                text: qsTr("Clear queue")
                visible: !playlist.refreshing && !playlist.busy && listView.count > 0
                onClicked: remorse.execute(qsTr("Clearing play queue"), function() { playlist.clear() } )
            }

            MenuItem {
                text: playlist.refreshing ? qsTr("Cancel") : qsTr("Refresh items")
                visible: playlist.refreshable && !playlist.busy && listView.count > 0
                onClicked: {
                    if (playlist.refreshing)
                        playlist.cancelRefresh()
                    else
                        playlist.refresh()
                }
            }

            MenuItem {
                text: playlist.busy ? qsTr("Cancel") : qsTr("Add items")
                visible: !playlist.refreshing && directory.inited
                onClicked: {
                    if (playlist.busy) {
                        playlist.cancelAdd()
                    } else {
                        pageStack.push(Qt.resolvedUrl("AddMediaPage.qml"))
                    }
                }
            }

            MenuLabel {
                visible: (playlist.busy || playlist.refreshing)
                text: playlist.refreshing ?
                          playlist.progressTotal > 1 ?
                              qsTr("Refreshing item %1 of %2...").arg(playlist.progressValue + 1).arg(playlist.progressTotal) :
                              qsTr("Refreshing item...") :
                          playlist.progressTotal > 1 ?
                              qsTr("Adding item %1 of %2...").arg(playlist.progressValue + 1).arg(playlist.progressTotal) :
                              qsTr("Adding item...")
            }
        }

        delegate: DoubleListItem {
            id: listItem

            property color primaryColor: highlighted || model.active ?
                                         (enabled ? Theme.highlightColor : Theme.secondaryHighlightColor) :
                                         (enabled ? Theme.primaryColor : Theme.secondaryColor)
            property color secondaryColor: highlighted || model.active ?
                                         Theme.secondaryHighlightColor : Theme.secondaryColor
            property bool isImage: model.type === AVTransport.T_Image

            enabled: !playlist.busy && !av.busy && !rc.busy && !playlist.refreshing
            opacity: enabled ? 1.0 : 0.8

            defaultIcon.source: {
                if (model.itemType === ContentServer.ItemType_Mic)
                    return "image://theme/icon-m-mic?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_AudioCapture)
                    return "image://theme/icon-m-speaker?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_ScreenCapture)
                    return "image://theme/icon-m-display?" + primaryColor
                else
                    switch (model.type) {
                    case AVTransport.T_Image:
                        return "image://theme/icon-m-file-image?" + primaryColor
                    case AVTransport.T_Audio:
                        return "image://theme/icon-m-file-audio?" + primaryColor
                    case AVTransport.T_Video:
                        return "image://theme/icon-m-file-video?" + primaryColor
                    default:
                        return "image://theme/icon-m-file-other?" + primaryColor
                    }
            }
            attachedIcon.source: model.itemType === ContentServer.ItemType_Url ?
                                     ("image://icons/icon-s-browser?" + primaryColor) :
                                 model.itemType === ContentServer.ItemType_Upnp ?
                                     ("image://icons/icon-s-device?" + primaryColor) : ""
            icon.source: {
                if (model.itemType === ContentServer.ItemType_Mic ||
                    model.itemType === ContentServer.ItemType_AudioCapture ||
                    model.itemType === ContentServer.ItemType_ScreenCapture) {
                    return ""
                }
                return model.icon
            }
            icon.visible: !model.toBeActive
            title.text: model.name
            title.color: primaryColor
            subtitle.text: model.artist.length > 0 ? model.artist : ""
            subtitle.color: secondaryColor

            onClicked: {
                if (!listItem.enabled)
                    return;
                if (!directory.inited)
                    return;
                if (root.devless) {
                    listItem.openMenu()
                } else {
                    if (model.active)
                        playlist.togglePlay()
                    else
                        playlist.play(model.id)
                }
            }

            menu: ContextMenu {
                MenuItem {
                    text: listItem.isImage ? qsTr("Show") : qsTr("Play")
                    enabled: model.active || listItem.enabled
                    visible: directory.inited && !root.devless &&
                             ((av.transportState !== AVTransport.Playing &&
                              !listItem.isImage) || !model.active)
                    onClicked: {
                        if (!model.active) {
                            playlist.play(model.id)
                        } else if (av.transportState !== AVTransport.Playing) {
                            playlist.togglePlay()
                        }
                    }
                }

                MenuItem {
                    text: qsTr("Pause")
                    enabled: model.active || listItem.enabled
                    visible: directory.inited && !root.devless &&
                             (av.stopable && model.active && !listItem.isImage)
                    onClicked: {
                        playlist.togglePlay()
                    }
                }

                MenuItem {
                    text: qsTr("Remove")
                    enabled: model.active || listItem.enabled
                    visible: directory.inited
                    onClicked: {
                        playlist.remove(model.id)
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: icon
                color: primaryColor
                running: model.toBeActive
                visible: running
                size: BusyIndicatorSize.Medium
            }
        }

        RemorsePopup {
            id: remorse
        }

        ViewPlaceholder {
            enabled: listView.count == 0 && !playlist.busy && !av.busy && !rc.busy
            text: qsTr("No items")
            hintText: qsTr("Pull down to add new items")
            verticalOffset: ppanel.open ? 0-ppanel.height/2 : 0
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: playlist.busy || av.busy || rc.busy
        size: BusyIndicatorSize.Large
    }

    PlayerPanel {
        id: ppanel

        open: !root.devless && Qt.application.active && !menu.active &&
              av.controlable && root.status === PageStatus.Active

        title: av.currentTitle.length === 0 ? qsTr("Unknown") : av.currentTitle
        subtitle: app.streamTitle.length === 0 ?
                      (root.itemType !== ContentServer.ItemType_Mic &&
                       root.itemType !== ContentServer.ItemType_AudioCapture &&
                       root.itemType !== ContentServer.ItemType_ScreenCapture ?
                           av.currentAuthor : "") : app.streamTitle
        itemType: root.itemType

        prevEnabled: playlist.prevSupported &&
                     !playlist.refreshing
        nextEnabled: playlist.nextSupported &&
                     !playlist.refreshing

        forwardEnabled: av.seekSupported &&
                        av.transportState === AVTransport.Playing &&
                        av.currentType !== AVTransport.T_Image
        backwardEnabled: forwardEnabled
        recordEnabled: app.streamRecordable
        recordActive: app.streamToRecord

        playMode: playlist.playMode

        controlable: av.controlable

        onRunningChanged: {
            if (open && !running)
                root.showActiveItem()
        }

        onNextClicked: playlist.next()
        onPrevClicked: playlist.prev()
        onTogglePlayClicked: playlist.togglePlay()

        onForwardClicked: {
            var pos = av.relativeTimePosition + settings.forwardTime
            var max = av.currentTrackDuration
            av.seek(pos > max ? max : pos)
        }

        onBackwardClicked: {
            var pos = av.relativeTimePosition - settings.forwardTime
            av.seek(pos < 0 ? 0 : pos)
        }

        onRepeatClicked: {
            playlist.togglePlayMode()
        }

        onRecordClicked: {
            cserver.setStreamToRecord(av.currentId, !app.streamToRecord)
        }
    }

    InteractionHintLabel_ {
        anchors.bottom: parent.bottom
        opacity: enabled ? 1.0 : 0.0
        Behavior on opacity { FadeAnimation {} }
        text: qsTr("Not connected")
        subtext: qsTr("Connect to a device to control playback using %1.").arg(APP_NAME)
                 + (settings.contentDirSupported ? " " + qsTr("Without connection, all items in play queue are still accessible on other devices in your local network.") : "")
        enabled: settings.hintEnabled(Settings.Hint_NotConnectedTip) && devless && !menu.active

        MouseArea {
            anchors.fill: parent
            onClicked: {
                settings.disableHint(Settings.Hint_NotConnectedTip)
                parent.enabled = false
            }
        }
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
