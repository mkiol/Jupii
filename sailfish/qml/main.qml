/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.jupii.YoutubeDl 1.0

ApplicationWindow {
    id: app

    cover: Qt.resolvedUrl("CoverPage.qml")
    allowedOrientations: Orientation.All

    initialPage: devPage

    Component {
        id: devPage
        DevicesPage {}
    }

    Notification {
        id: notification
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
    }
    Connections {
        target: cserver
        onStreamTitleChanged: updateStreamInfo()
        onStreamRecordableChanged: updateStreamInfo()
        onStreamToRecordChanged: updateStreamInfo()
        onStreamRecorded: {
            notification.show(qsTr("Track \"%1\" saved").arg(title))
        }
    }

    Binding {
        target: settings
        property: "colorScheme"
        value: Theme.colorScheme
    }

    Connections {
        target: directory
        onError: {
            switch(code) {
            case 1:
                notification.show(qsTr("Cannot connect to a local network"))
                break
            default:
                notification.show(qsTr("An internal error occurred"))
            }
        }
        onInitedChanged: {
            if (!directory.inited) {
                pageStack.clear()
                pageStack.completeAnimation()
                pageStack.push(devPage, {}, PageStackAction.Immediate)
            }
        }
    }

    Connections {
        target: ytdl
        onError : {
            switch(code) {
            case YoutubeDl.DownloadBin_Error:
                notification.show(qsTr("Cannot download youtube-dl"))
                break
            case YoutubeDl.UpdateBin_Error:
                notification.show(qsTr("Cannot update youtube-dl"))
                break
            case YoutubeDl.FindBin_Error:
                notification.show(qsTr("Cannot find URL with youtube-dl"))
                break
            }
        }
    }
}
