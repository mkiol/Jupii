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
            if (av.controlable) {
                pageStack.pushAttached(Qt.resolvedUrl("MediaInfoPage.qml"))
            } else {
                pageStack.popAttached(root, PageStackAction.Immediate)
            } /*else if (pageStack.depth === 3) {
                pageStack.popAttached(root, PageStackAction.Immediate)
            }*/
        }
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
//        onBcMainUrlAdded: {
//            pageStack.pop(root, PageStackAction.Immediate)
//            pageStack.push(Qt.resolvedUrl("BcPage.qml"))
//        }
//        onBcAlbumUrlAdded: {
//            pageStack.pop(root, PageStackAction.Immediate)
//            pageStack.push(Qt.resolvedUrl("BcPage.qml"), {albumPage: url})
//        }
//        onBcArtistUrlAdded: {
////            if (pageStack.currentPage !== root) {
////                pageStack.pop(root, PageStackAction.Immediate)
////            }
//            if (pageStack.busy)
//            replaceAbove(root, Qt.resolvedUrl("BcPage.qml"), {artistPage: url})
//            //pageStack.push(Qt.resolvedUrl("BcPage.qml"), {artistPage: url})
//        }
//        onSoundcloudMainUrlAdded: {
//            pageStack.pop(root, PageStackAction.Immediate)
//            pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"))
//        }
//        onSoundcloudAlbumUrlAdded: {
//            pageStack.pop(root, PageStackAction.Immediate)
//            pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {albumPage: url})
//        }
//        onSoundcloudArtistUrlAdded: {
//            pageStack.pop(root, PageStackAction.Immediate)
//            pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {artistPage: url})
//        }

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
                    pageStack.push(Qt.resolvedUrl("SavePlaylistPage.qml"), { playlist: playlist })
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
                    if (playlist.refreshing) playlist.cancelRefresh()
                    else playlist.refresh()
                }
            }

            MenuItem {
                text: playlist.busy ? qsTr("Cancel") : qsTr("Add items")
                visible: !playlist.refreshing
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

            dimmed: playlist.busy || av.busy || rc.busy || playlist.refreshing
            enabled: !dimmed && listView.count > 0

            defaultIcon.source: {
                if (model.itemType === ContentServer.ItemType_Mic)
                    return "image://theme/icon-m-mic?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_AudioCapture)
                    return "image://theme/icon-m-speaker?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_ScreenCapture)
                    return "image://theme/icon-m-display?" + primaryColor
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
        running: playlist.busy || av.busy || rc.busy
        text: (playlist.busy || playlist.refreshing) ?
                  "" + (playlist.progressValue + 1) + "/" + playlist.progressTotal : ""
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: playlist.busy || av.busy || rc.busy
        size: BusyIndicatorSize.Large

        Label {
            enabled: playlist.busy || playlist.refreshing
            opacity: enabled ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity { FadeAnimation {} }
            anchors.centerIn: parent
            font.pixelSize: Theme.fontSizeMedium
            color: parent.color
            text: "" + (playlist.progressValue + 1) + "/" + playlist.progressTotal
        }
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
        subtext: qsTr("Connect to a device to control playback using %1.").arg(APP_NAME)
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
