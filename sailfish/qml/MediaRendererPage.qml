/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0
import harbour.jupii.PlayListModel 1.0

Page {
    id: root

    property string deviceId

    property bool playable: av.inited && av.transportStatus === AVTransport.TPS_Ok &&
                            av.currentURI !== "" &&
                            av.playSupported &&
                            (av.transportState === AVTransport.Stopped ||
                            av.transportState === AVTransport.PausedPlayback ||
                            av.transportState === AVTransport.PausedRecording)
    property bool stopable: av.inited && av.transportStatus === AVTransport.TPS_Ok &&
                            av.currentURI !== "" &&
                            av.transportState === AVTransport.Playing &&
                            (av.pauseSupported || av.stopSupported)
    property bool controlable: playable || stopable
    property bool busy: av.busy || rc.busy
    property bool inited: av.inited && rc.inited

    property string image: av.transportStatus === AVTransport.TPS_Ok ? av.currentAlbumArtURI : ""
    property string label: av.transportStatus === AVTransport.TPS_Ok ?
                               av.currentAuthor.length > 0 ? av.currentAuthor + "\n" + av.currentTitle :
                               av.currentTitle : ""

    property bool _doPop: false;

    Component.onCompleted: {
        app.player = root
    }

    Component.onDestruction: {
        app.player = null
        dbus.canControl = false
    }

    onInitedChanged: {
        dbus.canControl = inited
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
            break
        }
    }

    function updatePlayList() {
        var count = listView.count

        if (count > 0) {
            var playing = av.transportState === AVTransport.Playing
            var apath = playlist.activePath()
            var npath = playlist.nextActivePath()

            if (apath.length === 0) {
                var fpath = playlist.firstPath()
                if (playing) {
                    av.setLocalContent("", fpath)
                } else {
                    av.setLocalContent(fpath, npath)
                }
                playlist.setActiveUrl(av.currentURI)
            } else {
                av.setLocalContent("",npath)
            }
        } else {
            av.clearNextUri()
        }
    }

    function playItem(path) {
        var count = listView.count

        if (count > 0) {
            var npath = playlist.nextPath(path)
            av.setLocalContent(path,npath)
        }
    }

    // -- Pickers --

    Component {
        id: filePickerPage
        FilePickerPage {
            nameFilters: cserver.getExtensions(settings.imageSupported ? 7 : 6)
            onSelectedContentPropertiesChanged: {
                playlist.addItems([selectedContentProperties.filePath])
            }
        }
    }

    Component {
        id: musicPickerDialog
        MultiMusicPickerDialog {
            onAccepted: {
                var urls = []
                for (var i = 0; i < selectedContent.count; ++i) {
                    var paths = selectedContent.get(i).filePath
                    playlist.addItems(paths)
                }
            }
        }
    }

    Component {
        id: videoPickerDialog
        MultiVideoPickerDialog {
            onAccepted: {
                var urls = []
                for (var i = 0; i < selectedContent.count; ++i) {
                    var paths = selectedContent.get(i).filePath
                    playlist.addItems(paths)
                }
            }
        }
    }

    Component {
        id: imagePickerDialog
        MultiImagePickerDialog {
            onAccepted: {
                var urls = []
                for (var i = 0; i < selectedContent.count; ++i) {
                    var paths = selectedContent.get(i).filePath
                    playlist.addItems(paths)
                }
            }
        }
    }

    // ----

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop)
                pageStack.pop(pageStack.pop());
        }
    }

    RenderingControl {
        id: rc

        Component.onCompleted: {
            init(deviceId)
        }

        onVolumeChanged: {
            volumeSlider.updateValue(volume)
        }

        onError: {
            handleError(code)
        }
    }

    AVTransport {
        id: av

        Component.onCompleted: {
            init(deviceId)
        }

        onRelativeTimePositionChanged: {
            ppanel.trackPositionSlider.updateValue(relativeTimePosition)
        }

        onError: {
            handleError(code)
        }

        onUpdated: {
            rc.asyncUpdate()
        }

        onUriChanged: {
            //console.log("UriChanged")
            //console.log("  currentURI: " + currentURI)
            //console.log("  nextURI: " + nextURI)
            playlist.setActiveUrl(currentURI)
            updatePlayList()
        }

        /*onTransportActionsChanged: {
            console.log("transportActionsChanged")
        }

        onSeekSupportedChanged: {
            console.log("SeekSupportedChanged: " + seekSupported)
        }

        onNextSupportedChanged: {
            console.log("NextSupportedChanged: " + nextSupported)
        }*/
    }

    Connections {
        target: dbus
        onRequestAppendPath: {
            playlist.addItems([path])
            notification.show("Item added to Jupii playlist")
        }
        onRequestClearPlaylist: {
            playlist.clear()
            root.updatePlayList()
        }
    }

    PlayListModel {
        id: playlist

        onItemsAdded: {
            root.updatePlayList()
        }

        onError: {
            if (code === PlayListModel.E_FileExists)
                notification.show(qsTr("Item is already in the playlist"))
        }
    }

    SilicaListView {
        id: listView

        width: parent.width
        height: ppanel.open ? parent.height - ppanel.height : parent.height

        clip: true

        opacity: root.busy ? 0.0 : 1.0
        visible: opacity > 0.0
        Behavior on opacity { FadeAnimator {} }

        model: playlist

        header: PageHeader {
            title: qsTr("Playlist")
        }

        VerticalScrollDecorator {}

        ViewPlaceholder {
            enabled: root.inited && !root.busy && listView.count == 0
            text: qsTr("Empty")
            verticalOffset: ppanel.open ? 0-ppanel.height/2 : 0
        }

        ViewPlaceholder {
            enabled: !root.inited && !root.busy
            text: qsTr("Not connected")
            verticalOffset: ppanel.open ? 0-ppanel.height/2 : 0
        }

        PullDownMenu {
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

                    //label: qsTr("Volume")

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
                text: qsTr("Clear playlist")
                visible: av.inited
                onClicked: {
                    playlist.clear()
                    root.updatePlayList()
                }
            }

            MenuItem {
                text: qsTr("Add item")
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("AddMediaPage.qml"), {
                                       musicPickerDialog: musicPickerDialog,
                                       videoPickerDialog: videoPickerDialog,
                                       imagePickerDialog: imagePickerDialog,
                                       filePickerPage: filePickerPage
                                   })
                }
            }
        }

        delegate: SimpleListItem {
            id: listItem

            property color primaryColor: highlighted || model.active ?
                                         Theme.highlightColor : Theme.primaryColor

            visible: root.inited && !root.busy

            icon.source: model.icon + "?" + primaryColor
            title.text: model.name
            title.color: primaryColor

            onClicked: {
                if (!model.active)
                    root.playItem(model.path)
            }

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Remove")
                    onClicked: {
                        playlist.remove(model.path)
                        root.updatePlayList()
                    }
                }

                MenuItem {
                    text: qsTr("Play")
                    visible: av.transportState !== AVTransport.Playing || !model.active
                    onClicked: {
                        var playing = av.transportState === AVTransport.Playing
                        if (!playing || !model.active)
                            root.playItem(model.path)
                    }
                }
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: root.busy
        size: BusyIndicatorSize.Large
    }

    PlayerPanel {
        id: ppanel

        open: root.controlable && !root.busy

        prevEnabled: playlist.activeItemIndex > 0 ||
                     (av.seekSupported && av.transportState === AVTransport.Playing)
        nextEnabled: av.nextSupported

        av: av
        rc: rc

        onLabelClicked: {
            pageStack.push(Qt.resolvedUrl("MediaInfoPage.qml"),{av: av})
        }

        onNextClicked: {
            var count = listView.count

            if (count > 0) {

                var fpath = playlist.firstPath()
                var apath = playlist.activePath()
                var npath = playlist.nextActivePath()

                if (apath.length === 0)
                    av.setLocalContent(fpath, npath)
                else
                    av.setLocalContent(npath, "")

            } else {
                console.log("Playlist is empty so can't do next()")
            }
        }

        onPrevClicked: {
            var count = listView.count
            var seekable = av.seekSupported

            if (count > 0) {
                var ppath = playlist.prevActivePath()
                var apath = playlist.activePath()

                if (apath.length === 0) {
                    if (seekable)
                        av.seek(0)
                    return
                }

                if (ppath.length === 0) {
                    if (seekable)
                        av.seek(0)
                    return
                }

                var seek = av.relativeTimePosition > 5
                if (seek && seekable)
                    av.seek(0)
                else
                    av.setLocalContent(ppath, apath)

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
    }
}
