/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0

ApplicationWindow {
    id: app

    cover: Qt.resolvedUrl("CoverPage.qml")
    allowedOrientations: Orientation.All

    initialPage: devPage

    Component.onCompleted: {
        console.log("QML completed");
        conn.update()
    }

    Component {
        id: devPage
        DevicesPage {}
    }

    function popToQueue() {
        pageStack.pop(queuePage(), PageStackAction.Immediate)
    }
    function popToDevices() {
        pageStack.pop(devicesPage(), PageStackAction.Immediate)
    }
    function queuePage() {
        return pageStack.find(function(page){return page.objectName === "queue"})
    }
    function devicesPage() {
        return pageStack.find(function(page){return page.objectName === "devices"})
    }
    function showQueue(force) {
        if (pageStack.busy) {
            console.log("pageStack busy")
            return false
        }

        var cname = pageStack.currentPage.objectName

        if (!force && (cname === "bc" || cname === "souncloud")) return false

        if (cname !== "queue") {
            popToDevices()
            if (pageStack.currentPage.forwardNavigation) {
                pageStack.navigateForward(PageStackAction.Immediate)
            } else {
                pageStack.push(Qt.resolvedUrl("PlayQueuePage.qml"), {}, PageStackAction.Immediate)
            }
        }

        return true
    }

    // -- stream --
    property string streamTitle: ""
    property var streamTitleHistory: []
    property bool streamRecordable: false
    property bool streamToRecord: false
    function updateStreamInfo() {
        streamTitle = cserver.streamTitle(av.currentId)
        streamTitleHistory = cserver.streamTitleHistory(av.currentId)
        streamRecordable = cserver.isStreamRecordable(av.currentId)
        streamToRecord = cserver.isStreamToRecord(av.currentId)
    }

    Connections {
        target: av
        onCurrentIdChanged: updateStreamInfo()
        onUpdated: rc.asyncUpdate()
        onInitedChanged: {
            if (av.inited && av.deviceFriendlyName.length > 0) {
                notifications.show(qsTr("Connected to %1").arg(av.deviceFriendlyName), "", av.deviceIconPath)
            }
        }

        onError: {
            switch(code) {
            case AVTransport.E_LostConnection:
            case AVTransport.E_NotInited:
                notifications.show(qsTr("Cannot connect to device"))
                popToDevices()
                break
            case AVTransport.E_ServerError:
                notifications.show(qsTr("Device responded with an error"))
                playlist.resetToBeActive()
                break
            case AVTransport.E_InvalidPath:
                notifications.show(qsTr("Cannot play the file"))
                playlist.resetToBeActive()
                break
            }
        }
    }

    Connections {
        target: cserver
        onStreamTitleChanged: updateStreamInfo()
        onStreamRecordableChanged: updateStreamInfo()
        onStreamToRecordChanged: updateStreamInfo()
        onStreamRecorded: {
            notifications.show(qsTr("Track \"%1\" saved").arg(title))
        }
    }

    Connections {
        target: directory
        onError: {
            switch(code) {
            case 1:
                notifications.show(qsTr("Cannot connect to a local network"))
                break
            default:
                notifications.show(qsTr("An internal error occurred"))
            }
        }
        onInitedChanged: {
            if (!directory.inited) {
                pageStack.clear()
                pageStack.completeAnimation()
                pageStack.push(devPage, {booted: true}, PageStackAction.Immediate)
            }
        }
    }

    Connections {
        target: dbus
        onFocusRequested: showQueue(false)
    }

    Connections {
        target: playlist

        onBcMainUrlAdded: {
            if (showQueue(true)) pageStack.push(Qt.resolvedUrl("BcPage.qml"))
        }
        onBcAlbumUrlAdded: {
            if (showQueue(true)) pageStack.push(Qt.resolvedUrl("BcPage.qml"), {albumPage: url})
        }
        onBcArtistUrlAdded: {
            if (showQueue(true)) pageStack.push(Qt.resolvedUrl("BcPage.qml"), {artistPage: url})
        }
        onSoundcloudMainUrlAdded: {
            if (showQueue(true)) pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"))
        }
        onSoundcloudAlbumUrlAdded: {
            if (showQueue(true)) pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {albumPage: url})
        }
        onSoundcloudArtistUrlAdded: {
            if (showQueue(true)) pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {artistPage: url})
        }
        onUnknownTypeUrlAdded: {
            if (showQueue(true)) pageStack.push(Qt.resolvedUrl("AddUrlPage.qml"), {url: url, name: name})
        }
    }
}
