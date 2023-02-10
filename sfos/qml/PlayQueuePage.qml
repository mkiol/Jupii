/* Copyright (C) 2017-2023 Michal Kosciesza <michal@mkiol.net>
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
    property alias selectionMode: selectionPanel.open
    property bool _doPop: false

    backgroundColor: selectionMode ? Theme.rgba(Theme.overlayBackgroundColor, Theme.opacityLow) : "transparent"

    onSelectionModeChanged: {
        updateMediaInfoPage()
        if (selectionMode) playlist.clearSelection()
    }

    onStatusChanged: {
        root.selectionMode = false

        if (status === PageStatus.Active) {
            playlist.clearSelection()
            settings.disableHint(Settings.Hint_DeviceSwipeLeft)
            updateMediaInfoPage()
            ppanel.update()
        }
    }

    function doPop() {
        if (pageStack.busy) _doPop = true;
        else pageStack.pop(pageStack.previousPage(root))
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
            if (av.controlable && !root.selectionMode) {
                pageStack.pushAttached(Qt.resolvedUrl("MediaInfoPage.qml"))
            } else if (root.forwardNavigation) {
                pageStack.popAttached(root, PageStackAction.Immediate)
            }
        }
    }

    function removeSelectedItems() {
        remorse.execute(qsTr("Removing items from play queue"),
                                                   function() {
                                                       playlist.removeSelectedItems()
                                                       root.selectionMode = false
                                                   } )
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop) {
                root._doPop = false
                pageStack.pop(pageStack.pop())
                return;
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
            else if (code === PlayListModel.E_ProxyError)
                notifications.show(qsTr("Unable to play item"))
            else
                notifications.show(qsTr("Unknown error"))
        }

        onActiveItemChanged: {
            root.showActiveItem()
        }

        onProgressValueChanged: {
            if ((playlist.busy || playlist.refreshing) && playlist.progressTotal > 1)
                busyIndicator.progressText = "" + (playlist.progressValue + 1) + "/" + playlist.progressTotal
        }
    }

    SilicaListView {
        id: listView

        width: parent.width
        height: selectionPanel.open ? selectionPanel.y :
                ppanel.open ? ppanel.y : parent.height

        clip: true

        model: playlist

        header: PageHeader {
            title: {
                if (root.selectionMode) {
                    return playlist.selectedCount === 0 ? qsTr("Select items") :
                           qsTr("%n selected", "", playlist.selectedCount)
                }

                return av.deviceFriendlyName.length > 0 ? av.deviceFriendlyName : qsTr("Play queue")
            }
        }

        VerticalScrollDecorator {}

        PullDownMenu {
            id: menu
            busy: playlist.busy || av.busy || rc.busy || directory.busy || playlist.refreshing

            MenuItem {
                text: qsTr("Exit selection mode")
                visible: !playlist.refreshing && !playlist.busy && listView.count > 0 &&
                         root.selectionMode
                onClicked: {
                    playlist.clearSelection()
                    root.selectionMode = false
                }
            }

            MenuItem {
                text: qsTr("Save selected items")
                enabled: playlist.selectedCount > 0
                visible: !playlist.refreshing && !playlist.busy && listView.count > 0 &&
                         root.selectionMode
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("SavePlaylistPage.qml"))
                }
            }

            MenuItem {
                text: qsTr("Remove selected items")
                visible: !playlist.refreshing && !playlist.busy && listView.count > 0 &&
                         root.selectionMode
                enabled: playlist.selectedCount > 0
                onClicked: root.removeSelectedItems()
            }

            MenuItem {
                visible: !playlist.refreshing && !playlist.busy && listView.count !== 0 &&
                         root.selectionMode
                text: playlist.selectedCount === listView.count ? qsTr("Unselect all") : qsTr("Select all")

                onClicked: {
                    if (playlist.selectedCount === listView.count)
                        playlist.clearSelection()
                    else
                        playlist.setAllSelected(true)
                }
            }

            MenuItem {
                text: playlist.refreshing ? qsTr("Cancel") : qsTr("Refresh")
                visible: playlist.refreshable && !playlist.busy && listView.count > 0 &&
                         !root.selectionMode
                onClicked: {
                    if (playlist.refreshing) playlist.cancelRefresh()
                    else playlist.refresh()
                }
            }

            MenuItem {
                text: qsTr("Select")
                visible: !playlist.refreshing && !playlist.busy &&
                         listView.count > 0 && !root.selectionMode
                onClicked: root.selectionMode = true
            }

            MenuItem {
                text: playlist.busy ? qsTr("Cancel") : qsTr("Add")
                visible: !playlist.refreshing && !root.selectionMode
                onClicked: {
                    if (playlist.busy) {
                        playlist.cancelAdd()
                    } else {
                        pageStack.push(Qt.resolvedUrl("AddMediaPage.qml"))
                    }
                }
            }

            MenuLabel {
                visible: playlist.busy || playlist.refreshing
                text: playlist.refreshing ?
                          playlist.progressTotal > 1 ?
                              qsTr("Preparing item %1 of %2...").arg(playlist.progressValue + 1).arg(playlist.progressTotal) :
                              qsTr("Preparing item...") :
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

            dimmed: playlist.busy || av.busy || rc.busy || playlist.refreshing
            enabled: !dimmed && listView.count > 0

            defaultIcon.source: {
                if (model.itemType === ContentServer.ItemType_Mic)
                    return "image://theme/icon-m-mic?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_PlaybackCapture)
                    return "image://theme/icon-m-speaker?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_ScreenCapture)
                    return "image://theme/icon-m-display?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_Cam)
                    return "image://theme/icon-m-browser-camera?" + primaryColor
                else {
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
            }
            attachedIcon.source: {
                switch(model.itemType) {
                case ContentServer.ItemType_Url:
                    return "image://icons/icon-s-browser?" + primaryColor
                case ContentServer.ItemType_Upnp:
                    return "image://icons/icon-s-device?" + primaryColor
                }
                return ""
            }
            attachedIcon2.source: {
                if (icon.status !== Image.Ready)
                    return ""
                return defaultIcon.source
            }
            icon.source: {
                if (model.itemType === ContentServer.ItemType_Mic ||
                    model.itemType === ContentServer.ItemType_PlaybackCapture ||
                    model.itemType === ContentServer.ItemType_ScreenCapture ||
                    model.itemType === ContentServer.ItemType_Cam) {
                    return ""
                }
                return model.icon
            }
            icon.visible: !model.toBeActive
            title.text: model.name
            title.color: primaryColor
            subtitle.text: {
                switch (model.itemType) {
                case ContentServer.ItemType_Cam:
                    return model.videoSource + " · " + model.videoOrientation + (model.audioSource.length !== 0 ? (" · " + model.audioSource) : "")
                case ContentServer.ItemType_PlaybackCapture:
                    return model.audioSource.length !== 0 && model.audioSourceMuted ? qsTr("Audio source muted") : ""
                case ContentServer.ItemType_ScreenCapture:
                    return model.audioSource.length !== 0 ? model.audioSourceMuted ? qsTr("Audio capture (audio source muted)") : qsTr("Audio capture") : ""
                case ContentServer.ItemType_Mic:
                    return ""
                default:
                    return model.artist.length !== 0 ? model.artist : ""
                }
            }
            subtitle.color: secondaryColor
            highlighted: (root.selectionMode && model.selected) || down || menuOpen

            onClicked: {
                if (root.selectionMode) {
                    var selected = model.selected
                    playlist.setSelected(model.index, !selected)
                    return
                }
                if (!listItem.enabled) return;
                if (!directory.inited) return;
                if (root.devless) {
                    listItem.openMenu()
                } else {
                    if (model.active) playlist.togglePlay()
                    else playlist.play(model.id)
                }
            }

            menu: ContextMenu {
                enabled: !root.selectionMode
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
                Label {
                    font.pixelSize: Theme.fontSizeTiny
                    color: Theme.secondaryColor
                    anchors.centerIn: parent
                    text: cserver.cacheProgressString
                    visible: parent.running && cserver.caching
                }
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

//    OpacityRampEffect {
//        sourceItem: listView
//        direction: OpacityRamp.TopToBottom
//        offset: 0.97
//        slope: 30
//    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        running: playlist.busy || av.busy || rc.busy
    }

    DockedPanel {
        id: selectionPanel

        open: false
        animationDuration: 100
        width: parent.width
        height: Theme.itemSizeLarge + Theme.paddingLarge
        dock: Dock.Bottom

        Row {
            anchors {
                verticalCenter: parent.verticalCenter
                leftMargin: Theme.horizontalPageMargin
                left: parent.left
            }
            spacing: Theme.paddingLarge

            IconButton {
               icon.source: "image://theme/icon-m-delete"
               enabled: playlist.selectedCount > 0
               onClicked: root.removeSelectedItems()
            }

            IconButton {
               icon.source: "image://theme/icon-m-media-playlists"
               enabled: playlist.selectedCount > 0
               onClicked: pageStack.push(Qt.resolvedUrl("SavePlaylistPage.qml"))
            }
        }

        IconButton {
            anchors {
                verticalCenter: parent.verticalCenter
                rightMargin: Theme.horizontalPageMargin
                right: parent.right
            }

            icon.source: "image://theme/icon-m-cancel"
            onClicked: {
                playlist.clearSelection()
                root.selectionMode = false
            }
        }
    }

    PlayerPanel {
        id: ppanel

        open: !root.devless && Qt.application.active && !menu.active &&
              av.controlable && root.status === PageStatus.Active && !root.selectionMode

        title: av.currentTitle.length === 0 ? qsTr("Unknown") : av.currentTitle
        subtitle: app.streamTitle.length === 0 ?
                      (root.itemType !== ContentServer.ItemType_Mic &&
                       root.itemType !== ContentServer.ItemType_PlaybackCapture &&
                       root.itemType !== ContentServer.ItemType_ScreenCapture &&
                       root.itemType !== ContentServer.ItemType_Cam ?
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

        onFullChanged: {
            if (full) {
                settings.disableHint(Settings.Hint_ExpandPlayerPanelTip)
                expandHintLabel.enabled = false
            }
        }
    }

    // ***************//
    // Tips and Hints //
    // ***************//

    // Not connected to UPnP device info

    InteractionHintLabel_ {
        id: connectedHintLabel
        anchors.bottom: parent.bottom
        opacity: enabled ? 1.0 : 0.0
        Behavior on opacity { FadeAnimation {} }
        text: qsTr("Not connected")
        subtext: qsTr("Connect to a device to control playback.")
                 + (settings.contentDirSupported ? " " + qsTr("Without connection, all items in play queue are still accessible on other devices in your local network.") : "")
        enabled: settings.hintEnabled(Settings.Hint_NotConnectedTip)
                 && devless && !menu.active

        MouseArea {
            anchors.fill: parent
            onClicked: {
                settings.disableHint(Settings.Hint_NotConnectedTip)
                parent.enabled = false
            }
        }
    }

    // Expand Player panel

    InteractionHintLabel_ {
        id: expandHintLabel
        invert: true
        anchors.top: parent.top
        opacity: enabled ? 1.0 : 0.0
        Behavior on opacity { FadeAnimation {} }
        text: qsTr("Tap to access playback & volume controls")
        enabled: settings.hintEnabled(Settings.Hint_ExpandPlayerPanelTip) &&
                 !connectedHintLabel.enabled && !ppanel.full && av.controlable && !menu.active
        onEnabledChanged: enabled ? expandHint.start() : expandHint.stop()
    }
    TapInteractionHint {
        id: expandHint
        loops: Animation.Infinite
        anchors.centerIn: ppanel
    }

    // Swipe left to Media info page

    InteractionHintLabel_ {
        id: mediaInfoHintLabel
        anchors.bottom: parent.bottom
        opacity: enabled ? 1.0 : 0.0
        Behavior on opacity { FadeAnimation {} }
        text: qsTr("Flick left to see current track details")
        enabled: settings.hintEnabled(Settings.Hint_MediaInfoSwipeLeft) &&
                 !connectedHintLabel.enabled && !expandHintLabel.enabled &&
                 !ppanel.full && pageStack.currentPage === root &&
                 forwardNavigation && !menu.active
        onEnabledChanged: enabled ? mediaInfoHint.start() : mediaInfoHint.stop()
    }
    TouchInteractionHint {
        id: mediaInfoHint
        interactionMode: TouchInteraction.Swipe
        direction: TouchInteraction.Left
        loops: Animation.Infinite
        anchors.centerIn: parent
    }

    // ---

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
