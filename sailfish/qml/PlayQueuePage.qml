/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import Nemo.DBus 2.0

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0
import harbour.jupii.PlayListModel 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    property bool devless: av.deviceId.length === 0 && rc.deviceId.length === 0
    property bool inited: av.inited && rc.inited
    property bool isMic: utils.isIdMic(av.currentURL)
    property bool isPulse: utils.isIdPulse(av.currentURL)

    property bool _doPop: false

    onStatusChanged: {
        if (status === PageStatus.Active) {
            updateMediaInfoPage()
            if (rc.inited)
                volumeSlider.updateValue(rc.volume)
        }
    }

    function togglePlay() {
        if (av.transportState !== AVTransport.Playing) {
            av.speed = 1
            av.play()
            //var id = av.currentId;
            //av.setLocalContent(id.toString(), "");
        } else {
            if (av.pauseSupported)
                av.pause()
            else
                av.stop()
        }
    }

    function doPop() {
        if (pageStack.busy)
            _doPop = true;
        else
            pageStack.pop(pageStack.previousPage(root))
    }

    function handleError(code) {
        switch(code) {
        case AVTransport.E_LostConnection:
        case RenderingControl.E_LostConnection:
            notification.show("Cannot connect to the device")
            doPop()
            break
        case AVTransport.E_ServerError:
        case RenderingControl.E_ServerError:
            notification.show("Device responded with an error")
            playlist.resetToBeActive()
            break

        case AVTransport.E_InvalidPath:
            notification.show("Cannot play the file")
            playlist.resetToBeActive()
            break
        }
    }

    function playItem(id, index) {
        var count = listView.count

        if (count > 0) {
            console.log("Play: index = " + index)
            playlist.setToBeActiveIndex(index)

            var nid = playlist.nextId(id)
            av.setLocalContent(id,nid)
        }
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

    // -- Pickers --

    Component {
        id: filePickerPage
        FilePickerPage {
            allowedOrientations: Orientation.All
            nameFilters: cserver.getExtensions(settings.imageSupported ? 263 : 262)
            onSelectedContentPropertiesChanged: {
                playlist.addItemPath(selectedContentProperties.filePath)
            }
        }
    }

    Component {
        id: musicPickerDialog
        MultiMusicPickerDialog {
            allowedOrientations: Orientation.All
            onAccepted: {
                var paths = [];
                for (var i = 0; i < selectedContent.count; ++i)
                    paths.push(selectedContent.get(i).filePath)
                playlist.addItemPaths(paths)
            }
        }
    }

    Component {
        id: albumPickerPage
        AlbumsPage {
            onAccepted: {
               playlist.addItemUrls(items)
            }
        }
    }

    Component {
        id: artistPickerPage
        ArtistPage {
            onAccepted: {
                playlist.addItemUrls(items);
            }
        }
    }

    Component {
        id: playlistPickerPage
        PlaylistPage {
            onAccepted: {
                playlist.addItemUrls(items);
            }
        }
    }

    Component {
        id: videoPickerDialog
        MultiVideoPickerDialog {
            allowedOrientations: Orientation.All
            onAccepted: {
                var paths = [];
                for (var i = 0; i < selectedContent.count; ++i)
                    paths.push(selectedContent.get(i).filePath)
                playlist.addItemPaths(paths)
            }
        }
    }

    Component {
        id: audioFromVideoPickerDialog
        MultiVideoPickerDialog {
            allowedOrientations: Orientation.All
            onAccepted: {
                var paths = [];
                for (var i = 0; i < selectedContent.count; ++i)
                    paths.push(selectedContent.get(i).filePath)
                playlist.addItemPathsAsAudio(paths)
            }
        }
    }

    Component {
        id: imagePickerDialog
        MultiImagePickerDialog {
            allowedOrientations: Orientation.All
            onAccepted: {
                var paths = [];
                for (var i = 0; i < selectedContent.count; ++i)
                    paths.push(selectedContent.get(i).filePath)
                playlist.addItemPaths(paths)
            }
        }
    }

    Component {
        id: urlPickerPage
        AddUrlPage {
            onAccepted: {
                playlist.addItemUrl(url, name);
            }
        }
    }

    Component {
        id: somafmPickerPage
        SomafmPage {
            onAccepted: {
                playlist.addItemUrls(selectedItems);
            }
        }
    }

    Component {
        id: icecastPickerPage
        IcecastPage {
            onAccepted: {
                playlist.addItemUrls(selectedItems);
            }
        }
    }

    Component {
        id: gpodderPickerPage
        GpodderEpisodesPage {
            onAccepted: {
                playlist.addItemUrls(selectedItems)
            }
        }
    }

    Component {
        id: recPickerPage
        RecPage {
            onAccepted: {
                playlist.addItemUrls(selectedItems);
            }
        }
    }

    Component {
        id: upnpPickerPage
        UpnpCDirDevicesPage {
            onAccepted: {
                playlist.addItemUrls(items)
            }
        }
    }

    // ----

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
        target: rc

        onVolumeChanged: {
            volumeSlider.updateValue(rc.volume)
        }
        onError: {
            handleError(code)
        }
        onInitedChanged: {
            if (rc.inited)
                volumeSlider.updateValue(rc.volume)
        }
    }

    Connections {
        target: av

        onControlableChanged: {
            console.log("onControlableChanged: " + av.controlable)
            console.log(" playable: " + av.playable)
            console.log(" stopable: " + av.stopable)
            console.log(" av.transportStatus: " + av.transportStatus)
            console.log(" av.currentURI: " + av.currentURI)
            console.log(" av.playSupported: " + av.playSupported)
            console.log(" av.pauseSupported: " + av.pauseSupported)
            console.log(" av.playSupported: " + av.playSupported)
            console.log(" av.stopSupported: " + av.stopSupported)

            updateMediaInfoPage()
        }
        onError: {
            handleError(code)
        }
        onUpdated: {
            rc.asyncUpdate()
        }
    }

    Connections {
        target: playlist

        onItemsAdded: {
            root.showLastItem()
        }

        onItemsLoaded: {
            root.showActiveItem()
        }

        onError: {
            if (code === PlayListModel.E_FileExists)
                notification.show(qsTr("Item is already added"))
            else if (code === PlayListModel.E_ItemNotAdded)
                notification.show(qsTr("Item cannot be added"))
            else if (code === PlayListModel.E_SomeItemsNotAdded)
                notification.show(qsTr("Some items cannot be added"))
            else if (code === PlayListModel.E_AllItemsNotAdded)
                notification.show(qsTr("Items cannot be added"))
            else
                notification.show(qsTr("Unknown error"))
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

        visible: opacity > 0.0
        Behavior on opacity { FadeAnimation {} }

        model: playlist

        header: PageHeader {
            title: av.deviceFriendlyName.length > 0 ? av.deviceFriendlyName : qsTr("Play queue")
        }

        VerticalScrollDecorator {}

        ViewPlaceholder {
            enabled: listView.count == 0 && !playlist.busy
            text: qsTr("Empty")
            verticalOffset: ppanel.open ? 0-ppanel.height/2 : 0
        }

        PullDownMenu {
            id: menu
            enabled: !playlist.busy
            busy: !enabled && !av.busy && !rc.busy

            Item {
                width: parent.width
                height: volumeSlider.height
                visible: rc.inited

                Slider {
                    id: volumeSlider

                    property bool blockValueChangedSignal: false

                    anchors {
                        left: parent.left
                        leftMargin: muteButt.width/1.5
                        right: parent.right
                    }

                    minimumValue: 0
                    maximumValue: 100
                    stepSize: settings.volStep
                    valueText: value
                    opacity: rc.mute ? 0.7 : 1.0

                    onValueChanged: {
                        if (!blockValueChangedSignal) {
                            rc.volume = value
                            rc.setMute(false)
                        }
                    }

                    function updateValue(_value) {
                        blockValueChangedSignal = true
                        value = _value
                        blockValueChangedSignal = false
                    }
                }

                IconButton {
                    id: muteButt
                    anchors {
                        left: parent.left
                        leftMargin: Theme.horizontalPageMargin
                        bottom: parent.bottom
                    }
                    icon.source: rc.mute ? "image://theme/icon-m-speaker-mute":
                                           "image://theme/icon-m-speaker"
                    onClicked: rc.setMute(!rc.mute)
                }
            }

            MenuItem {
                text: qsTr("Save queue")
                visible: !playlist.busy && listView.count > 0
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("SavePlaylistPage.qml"), {
                                       playlist: playlist
                                   })
                }
            }

            MenuItem {
                text: qsTr("Clear queue")
                visible: !playlist.busy && listView.count > 0
                onClicked: remorse.execute("Clearing play queue", function() { playlist.clear() } )
            }

            MenuItem {
                text: qsTr("Add item")
                enabled: !playlist.busy
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("AddMediaPage.qml"), {
                                       musicPickerDialog: musicPickerDialog,
                                       videoPickerDialog: videoPickerDialog,
                                       audioFromVideoPickerDialog: audioFromVideoPickerDialog,
                                       imagePickerDialog: imagePickerDialog,
                                       albumPickerPage: albumPickerPage,
                                       artistPickerPage: artistPickerPage,
                                       playlistPickerPage: playlistPickerPage,
                                       filePickerPage: filePickerPage,
                                       urlPickerPage: urlPickerPage,
                                       somafmPickerPage: somafmPickerPage,
                                       icecastPickerPage: icecastPickerPage,
                                       gpodderPickerPage: gpodderPickerPage,
                                       recPickerPage: recPickerPage,
                                       upnpPickerPage: upnpPickerPage
                                   })
                }
            }
        }

        delegate: DoubleListItem {
            id: listItem

            property color primaryColor: highlighted || model.active ?
                                         Theme.highlightColor : Theme.primaryColor
            property color secondaryColor: highlighted || model.active ?
                                         Theme.secondaryHighlightColor : Theme.secondaryColor
            property bool isImage: model.type === AVTransport.T_Image
            property bool isMic: utils.isIdMic(model.id)
            property bool isPulse: utils.isIdPulse(model.id)
            property bool isScreen: utils.isIdScreen(model.id)

            opacity: playlist.busy ? 0.0 : 1.0
            visible: opacity > 0.0
            Behavior on opacity { FadeAnimation {} }

            defaultIcon.source: {
                if (isMic)
                    return "image://theme/icon-m-mic?" + primaryColor
                else if (isPulse)
                    return "image://theme/icon-m-speaker?" + primaryColor
                else if (isScreen)
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
            icon.source: isMic || isPulse || isScreen ? "" : model.icon
            icon.visible: !model.toBeActive
            title.text: model.name
            title.color: primaryColor
            subtitle.text: model.artist.length > 0 ? model.artist : ""
            subtitle.color: secondaryColor

            onClicked: {
                if (root.devless) {
                    listItem.openMenu()
                } else {
                    if (model.active)
                        root.togglePlay()
                    else
                        root.playItem(model.id, model.index)
                }
            }

            menu: ContextMenu {
                MenuItem {
                    text: listItem.isImage ? qsTr("Show") : qsTr("Play")
                    visible: !root.devless && ((av.transportState !== AVTransport.Playing &&
                              !listItem.isImage) || !model.active)
                    onClicked: {
                        if (!model.active) {
                            root.playItem(model.id, model.index)
                        } else if (av.transportState !== AVTransport.Playing) {
                            root.togglePlay()
                        }
                    }
                }

                MenuItem {
                    text: qsTr("Pause")
                    visible: !root.devless && (av.stopable && model.active && !listItem.isImage)
                    onClicked: {
                        root.togglePlay()
                    }
                }

                MenuItem {
                    text: qsTr("Remove")
                    onClicked: {
                        playlist.remove(model.id)
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: icon
                color: primaryColor
                running: model.toBeActive
                visible: model.toBeActive
                size: BusyIndicatorSize.Medium
            }
        }

        RemorsePopup {
            id: remorse
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: playlist.busy
        size: BusyIndicatorSize.Large
    }

    PlayerPanel {
        id: ppanel

        open: !root.devless && Qt.application.active && !menu.active &&
              av.controlable && root.status === PageStatus.Active

        title: av.currentTitle.length === 0 ? qsTr("Unknown") : av.currentTitle
        subtitle: app.streamTitle.length === 0 ?
                      (!root.isMic && !root.isPulse ? av.currentAuthor : "") : app.streamTitle

        prevEnabled: playlist.prevSupported
        nextEnabled: playlist.nextSupported

        forwardEnabled: av.seekSupported &&
                        av.transportState === AVTransport.Playing &&
                        av.currentType !== AVTransport.T_Image
        backwardEnabled: forwardEnabled
        recordEnabled: app.streamRecordable
        recordActive: app.streamToRecord

        playMode: playlist.playMode

        //controlable: av.controlable && av.currentType !== AVTransport.T_Image
        controlable: av.controlable

        onRunningChanged: {
            if (open && !running)
                root.showActiveItem()
        }

        onNextClicked: playlist.next()
        onPrevClicked: playlist.prev()
        onTogglePlayClicked: root.togglePlay()

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

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
