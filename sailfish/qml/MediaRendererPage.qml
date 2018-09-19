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

    property string deviceId
    property string deviceName

    property bool busy: av.busy || rc.busy
    property bool inited: av.inited && rc.inited
    property bool _doPop: false

    Component.onCompleted: {
        rc.init(deviceId)
        av.init(deviceId)

        volumeSlider.updateValue(rc.volume)
    }

    Component.onDestruction: {
        dbus.canControl = false
    }

    onInitedChanged: {
        dbus.canControl = inited

        if (inited) {
            if (listView.count === 0) {
                if (settings.rememberPlaylist)
                    playlist.load()
            }
        }
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            updateMediaInfoPage()
    }

    function togglePlay() {
        if (av.transportState !== AVTransport.Playing) {
            av.speed = 1
            av.play()
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
            notification.show("Can't connect to the device")
            doPop()
            break
        case AVTransport.E_ServerError:
        case RenderingControl.E_ServerError:
            notification.show("Device responded with an error")
            playlist.resetToBeActive()
            break

        case AVTransport.E_InvalidPath:
            notification.show("Can't play the file")
            playlist.resetToBeActive()
            break
        }
    }

    function updatePlayList(play) {
        var count = listView.count

        console.log("updatePlayList, count:" + count)

        if (count > 0) {
            var aid = playlist.activeId()

            console.log("updatePlayList, play:" + play)
            console.log("updatePlayList, aid:" + aid)

            if (aid.length === 0) {
                var fid = playlist.firstId()
                console.log("updatePlayList, fid:" + fid)

                if (play) {
                    var sid = playlist.secondId()
                    console.log("updatePlayList, sid:" + sid)
                    av.setLocalContent(fid, sid)
                } else {
                    if (fid.length > 0)
                        av.setLocalContent("", fid)
                }

            } else {
                var nid = playlist.nextActiveId()
                console.log("updatePlayList, nid:" + nid)
                av.setLocalContent("",nid)
            }
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
        if (av.controlable) {
            if (pageStack.currentPage === root)
                pageStack.pushAttached(Qt.resolvedUrl("MediaInfoPage.qml"))
        } else {
            pageStack.popAttached(root, PageStackAction.Immediate)
        }
    }

    DBusVolumeAgent {
        id: volumeAgent

        onVolumeChanged: {
            console.log("DBus volume is: " + volume)

            if (rc.inited && settings.useDbusVolume)
                rc.volume = volume
        }
    }

    // -- Pickers --

    Component {
        id: filePickerPage
        FilePickerPage {
            nameFilters: cserver.getExtensions(settings.imageSupported ? 7 : 6)
            onSelectedContentPropertiesChanged: {
                playlist.addItemPaths([selectedContentProperties.filePath])
            }
        }
    }

    Component {
        id: musicPickerDialog
        MultiMusicPickerDialog {
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
                playlist.addItemPaths(songs);
            }
        }
    }

    Component {
        id: artistPickerPage
        ArtistPage {
            onAccepted: {
                playlist.addItemPaths(songs);
            }
        }
    }

    Component {
        id: playlistPickerPage
        PlaylistPage {
            onAccepted: {
                playlist.addItemUrls(songs);
            }
        }
    }

    Component {
        id: videoPickerDialog
        MultiVideoPickerDialog {
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
                playlist.addItemUrl(url);
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
            if (rc.inited && settings.useDbusVolume)
                rc.volume = volumeAgent.volume
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

        onTrackEnded: {
            console.log("onTrackEnded")

            if (listView.count > 0 && (av.nextURISupported ||
                playlist.playMode === PlayListModel.PM_RepeatOne))
            {
                var aid = playlist.activeId()
                console.log("current id is: " + aid)
                if (aid.length > 0) {
                    var nid = playlist.nextActiveId()
                    console.log("next id will be: " + nid)
                    if (nid.length > 0)
                        av.setLocalContent(nid,"");
                }
            }
        }

        onCurrentURIChanged: {
            console.log("onCurrentURIChanged")
            playlist.setActiveUrl(av.currentURI)
            root.updatePlayList(av.transportState !== AVTransport.Playing &&
                                listView.count === 1)
        }

        onNextURIChanged: {
            console.log("onNextURIChanged")

            if (av.nextURI.length === 0 && av.currentURI.length > 0 &&
                    playlist.playMode !== PlayListModel.PM_Normal) {
                console.log("AVT switches to nextURI without currentURIChanged")
                playlist.setActiveUrl(av.currentURI)
                root.updatePlayList(av.transportState !== AVTransport.Playing &&
                                    listView.count === 1)
            }
        }

        onTransportStateChanged: {
            console.log("onTransportStateChanged: " + av.transportState)
            root.updatePlayList(false)
        }

        onTransportStatusChanged: {
            console.log("onTransportStatusChanged: " + av.transportStatus)
        }
    }

    Connections {
        target: dbus
        onRequestAppendPath: {
            playlist.addItems([path])
            notification.show("Item added to Jupii playlist")
        }

        onRequestClearPlaylist: {
            playlist.clear()
            root.updatePlayList(false)
        }
    }

    PlayListModel {
        id: playlist

        onItemsAdded: {
            console.log("onItemsAdded")
            root.showLastItem()
            playlist.setActiveUrl(av.currentURI)
            root.updatePlayList(av.transportState !== AVTransport.Playing &&
                                listView.count === 1)
        }

        onItemsLoaded: {
            console.log("onItemsLoaded")
            playlist.setActiveUrl(av.currentURI)
            root.showActiveItem()
            root.updatePlayList(false)
        }

        onItemRemoved: {
            console.log("onItemRemoved")
            root.updatePlayList(false)
        }

        onError: {
            if (code === PlayListModel.E_FileExists)
                notification.show(qsTr("Item is already in the playlist"))
        }

        onActiveItemChanged: {
            root.showActiveItem()
        }

        onPlayModeChanged: {
            console.log("onPlayModeChanged")
            root.updatePlayList(false)
        }
    }

    SilicaListView {
        id: listView

        width: parent.width
        height: ppanel.open ? ppanel.y : parent.height

        clip: true

        opacity: root.busy ? 0.0 : 1.0
        visible: opacity > 0.0
        Behavior on opacity { FadeAnimation {} }

        model: playlist

        header: PageHeader {
            title: root.deviceName.length > 0 ? root.deviceName : qsTr("Playlist")
        }

        VerticalScrollDecorator {}

        ViewPlaceholder {
            enabled: root.inited && !root.busy && listView.count == 0 && !playlist.busy
            text: qsTr("Empty")
            verticalOffset: ppanel.open ? 0-ppanel.height/2 : 0
        }

        ViewPlaceholder {
            enabled: !root.inited && !root.busy
            text: qsTr("Not connected")
            verticalOffset: ppanel.open ? 0-ppanel.height/2 : 0
        }

        PullDownMenu {
            id: menu

            enabled: root.inited && !root.busy

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
                    stepSize: 1
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
                text: qsTr("Save playlist")
                visible: av.inited && !playlist.busy && listView.count > 0
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("SavePlaylistPage.qml"), {
                                       playlist: playlist
                                   })
                }
            }

            MenuItem {
                text: qsTr("Clear playlist")
                visible: av.inited && !playlist.busy && listView.count > 0
                onClicked: {
                    playlist.clear()
                    root.updatePlayList(false)
                }
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
                                       urlPickerPage: urlPickerPage
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

            opacity: playlist.busy ? 0.0 : 1.0
            visible: opacity > 0.0
            Behavior on opacity { FadeAnimation {} }

            defaultIcon.source: {
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
            icon.source: model.icon
            icon.visible: !model.toBeActive
            title.text: model.name
            title.color: primaryColor
            /*subtitle.text: {
                var t = model.artist;
                var d = model.duration;
                if (d > 0)
                    t += (" • " + utils.secToStr(d))
                return t;
            }*/
            subtitle.text: model.artist.length > 0 ? model.artist : ""
            subtitle.color: secondaryColor

            onClicked: {
                if (model.active)
                    root.togglePlay()
                else
                    root.playItem(model.id, model.index)
            }

            menu: ContextMenu {
                MenuItem {
                    text: listItem.isImage ? qsTr("Show") : qsTr("Play")
                    visible: (av.transportState !== AVTransport.Playing &&
                              !listItem.isImage) || !model.active
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
                    visible: av.stopable && model.active && !listItem.isImage
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
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: root.busy || playlist.busy
        size: BusyIndicatorSize.Large
    }

    PlayerPanel {
        id: ppanel

        open: Qt.application.active &&
              !menu.active && av.controlable &&
              !root.busy && root.status === PageStatus.Active

        prevEnabled: (av.seekSupported &&
                      av.transportState === AVTransport.Playing &&
                      av.relativeTimePosition > 5) ||
                     (playlist.playMode !== PlayListModel.PM_RepeatOne &&
                      playlist.activeItemIndex > -1)

        nextEnabled: playlist.playMode !== PlayListModel.PM_RepeatOne &&
                     av.nextSupported && listView.count > 0
        forwardEnabled: av.seekSupported &&
                        av.transportState === AVTransport.Playing &&
                        av.currentType !== AVTransport.T_Image
        backwardEnabled: forwardEnabled

        playMode: playlist.playMode

        controlable: av.controlable && av.currentType !== AVTransport.T_Image

        onRunningChanged: {
            if (open && !running)
                root.showActiveItem()
        }

        onNextClicked: {
            var count = listView.count

            if (count > 0) {

                var fid = playlist.firstId()
                var aid = playlist.activeId()
                var nid = playlist.nextActiveId()

                if (aid.length === 0)
                    av.setLocalContent(fid, nid)
                else
                    av.setLocalContent(nid, "")

            } else {
                console.log("Playlist is empty so can't do next()")
            }
        }

        onPrevClicked: {
            var count = listView.count
            var seekable = av.seekSupported

            if (count > 0) {
                var pid = playlist.prevActiveId()
                var aid = playlist.activeId()

                if (aid.length === 0) {
                    if (seekable)
                        av.seek(0)
                    return
                }

                if (pid.length === 0) {
                    if (seekable)
                        av.seek(0)
                    return
                }

                var seek = av.relativeTimePosition > 5
                if (seek && seekable)
                    av.seek(0)
                else
                    av.setLocalContent(pid, aid)

            } else {
                if (seekable)
                    av.seek(0)
            }
        }

        onTogglePlayClicked: {
            root.togglePlay()
        }

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
    }
}
