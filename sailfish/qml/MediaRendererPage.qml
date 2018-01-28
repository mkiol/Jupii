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

    // -- Needed for Cover --
    property bool playable: av.playable
    property bool stopable: av.stopable
    property string image: av.transportStatus === AVTransport.TPS_Ok ?
                               av.currentAlbumArtURI : ""
    property string title: av.currentTitle.length > 0 ? av.currentTitle : qsTr("Unknown")
    property string author: av.currentAuthor.length > 0 ? av.currentAuthor : ""
    // ----

    property bool _doPop: false
    property int dbusVolume: -1

    Component.onCompleted: {
        app.player = root
    }

    Component.onDestruction: {
        app.player = null
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

    // -- D-Bus volume --

    onDbusVolumeChanged: {
        console.log("onDbusVolumeChanged: " + dbusVolume)

        if (rc.inited && settings.useDbusVolume)
            rc.volume = dbusVolume
    }

    DBusInterface {
        service: "com.nokia.profiled"
        path: "/com/nokia/profiled"
        iface: "com.nokia.profiled"

        signalsEnabled: true

        Component.onCompleted: getVolume()

        function updateDbusVolume(obj) {
            if (obj.length > 1) {
                if (obj[0] === "ringing.alert.volume") {
                    var volume = obj[1]
                    if (volume >= 0 && volume <= 100) {
                        //console.log("D-Bus volume is " + volume)
                        root.dbusVolume = volume
                    }
                }
            }
        }

        function profile_changed(changed, active, name, obj) {
            /*console.log("com.nokia.profiled profile_changed signal")
            console.log(" changed: " + changed)
            console.log(" active: " + active)
            console.log(" name: " + name)
            console.log(" obj: " + obj)*/

            if (name === "general" && obj.length === 1)
                updateDbusVolume(obj[0])
        }

        function getVolume() {
            typedCall("get_values", {"type": "s", "value": "general"},
              function(result) {
                  console.log("D-Bus getVolume call completed")
                  var l = result.length
                  for (var i = 0; i < l; i++)
                      updateDbusVolume(result[i])
              },
              function() {
                  console.log("D-Bus getVolume call failed")
              })
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
                var paths = [];
                for (var i = 0; i < selectedContent.count; ++i)
                    paths.push(selectedContent.get(i).filePath)
                playlist.addItems(paths)
            }
        }
    }

    Component {
        id: albumPickerPage
        AlbumsPage {
            onAccepted: {
                playlist.addItems(songs);
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
                playlist.addItems(paths)
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
                playlist.addItems(paths)
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

        onInitedChanged: {
            if (inited && settings.useDbusVolume)
                volume = root.dbusVolume
        }
    }

    AVTransport {
        id: av

        Component.onCompleted: {
            init(deviceId)
        }

        onControlableChanged: {
            console.log("onControlableChanged: " + controlable)
            console.log(" playable: " + playable)
            console.log(" stopable: " + stopable)
            console.log(" av.transportStatus: " + av.transportStatus)
            console.log(" av.currentURI: " + av.currentURI)
            console.log(" av.playSupported: " + av.playSupported)
            console.log(" av.pauseSupported: " + av.pauseSupported)
            console.log(" av.playSupported: " + av.playSupported)
            console.log(" av.stopSupported: " + av.stopSupported)

            // Media info page
            if (controlable) {
                pageStack.pushAttached(Qt.resolvedUrl("MediaInfoPage.qml"),{av: av})
            } else {
                pageStack.popAttached(null, PageStackAction.Immediate)
            }
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

        onCurrentURIChanged: {
            console.log("onCurrentURIChanged")
            playlist.setActiveUrl(currentURI)
            root.updatePlayList(av.transportState !== AVTransport.Playing &&
                                listView.count === 1)
        }

        onNextURIChanged: {
            console.log("onNextURIChanged")

            if (av.nextURI.length === 0 && av.currentURI.length > 0 &&
                    playlist.playMode !== PlayListModel.PM_Normal) {
                console.log("AVT switches to nextURI without currentURIChanged")
                playlist.setActiveUrl(currentURI)
                root.updatePlayList(av.transportState !== AVTransport.Playing &&
                                    listView.count === 1)
            }
        }

        onTransportStateChanged: {
            console.log("onTransportStateChanged: " + transportState)
            root.updatePlayList(false)
        }

        onTransportStatusChanged: {
            console.log("onTransportStatusChanged: " + transportStatus)
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
        Behavior on opacity { FadeAnimator {} }

        model: playlist

        header: PageHeader {
            title: root.deviceName.length > 0 ? root.deviceName : qsTr("Playlist")
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
                    root.updatePlayList(false)
                }
            }

            MenuItem {
                text: qsTr("Add item")
                onClicked: {
                    pageStack.push(Qt.resolvedUrl("AddMediaPage.qml"), {
                                       musicPickerDialog: musicPickerDialog,
                                       videoPickerDialog: videoPickerDialog,
                                       imagePickerDialog: imagePickerDialog,
                                       albumPickerPage: albumPickerPage,
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
            icon.visible: !model.toBeActive
            title.text: model.name
            title.color: primaryColor

            onClicked: {
                if (model.active)
                    root.togglePlay()
                else
                    root.playItem(model.id, model.index)
            }

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Play")
                    visible: av.transportState !== AVTransport.Playing || !model.active
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
                    visible: av.stopable && model.active
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
        running: root.busy
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
        forwardEnabled: av.seekSupported && av.transportState === AVTransport.Playing
        backwardEnabled: forwardEnabled

        playMode: playlist.playMode

        av: av
        rc: rc

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
            switch(playlist.playMode) {
            case PlayListModel.PM_Normal:
                playlist.playMode = PlayListModel.PM_RepeatAll
                break;
            case PlayListModel.PM_RepeatAll:
                playlist.playMode = PlayListModel.PM_RepeatOne
                break;
            case PlayListModel.PM_RepeatOne:
                playlist.playMode = PlayListModel.PM_Normal
                break;
            default:
                playlist.playMode = PlayListModel.PM_Normal
            }
        }
    }
}
